#include "PuffyVirtualAudio.hpp"

#include <ntintsafe.h>

namespace puffy::virtual_audio::driver {
namespace {

// Stable identifiers for the two endpoint circuits owned by this driver.
// They are intentionally Puffy-specific and are not copied from a sample driver.
constexpr GUID GUID_PUFFY_TRANSPORT_COMPONENT =
    {0x94939d6e, 0x66b1, 0x45d7, {0x8e, 0xe0, 0xf8, 0x8b, 0xc1, 0xd0, 0x38, 0x5f}};
constexpr GUID GUID_PUFFY_MICROPHONE_COMPONENT =
    {0x57ee6105, 0x78cd, 0x4d0f, {0x9a, 0xb6, 0x59, 0xfa, 0xce, 0xad, 0x35, 0xac}};

[[nodiscard]] constexpr ULONG minUlong(ULONG left, ULONG right) noexcept {
    return left < right ? left : right;
}


[[nodiscard]] constexpr ULONG nonZeroUlong(ULONG value) noexcept {
    return value == 0 ? 1 : value;
}

inline constexpr ULONGLONG kHnsPerSecond = 10'000'000ULL;
inline constexpr ULONG kTimerQuantumMilliseconds = 2;
inline constexpr ULONG kTimerQuantumBytes =
    (kAverageBytesPerSecond * kTimerQuantumMilliseconds) / 1000;
static_assert(kTimerQuantumBytes >= kBlockAlign);
static_assert((kTimerQuantumBytes % kBlockAlign) == 0);

[[nodiscard]] ULONGLONG qpcToHns(LONGLONG qpc, LONGLONG frequency) noexcept {
    if (qpc <= 0 || frequency <= 0) return 0;
    const ULONGLONG uqpc = static_cast<ULONGLONG>(qpc);
    const ULONGLONG ufreq = static_cast<ULONGLONG>(frequency);
    const ULONGLONG wholeSeconds = uqpc / ufreq;
    const ULONGLONG remainder = uqpc % ufreq;
    return (wholeSeconds * kHnsPerSecond) + ((remainder * kHnsPerSecond) / ufreq);
}

[[nodiscard]] ULONGLONG bytesToHns(ULONGLONG bytes, ULONG bytesPerSecond) noexcept {
    if (bytesPerSecond == 0) return 0;
    const ULONGLONG rate = bytesPerSecond;
    const ULONGLONG wholeSeconds = bytes / rate;
    const ULONGLONG remainder = bytes % rate;
    return (wholeSeconds * kHnsPerSecond) + ((remainder * kHnsPerSecond) / rate);
}

const KSDATAFORMAT_WAVEFORMATEXTENSIBLE kPuffyFloat48Stereo = {
    {
        sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE),
        0,
        0,
        0,
        STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT),
        STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX),
    },
    {
        {
            WAVE_FORMAT_EXTENSIBLE,
            kChannels,
            kSampleRate,
            kAverageBytesPerSecond,
            kBlockAlign,
            kBitsPerSample,
            sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX),
        },
        {kBitsPerSample},
        KSAUDIO_SPEAKER_STEREO,
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT),
    },
};

[[nodiscard]] ULONG ringFreeBytes(const DEVICE_CONTEXT& ctx) noexcept {
    return kRingCapacityBytes - ctx.ringUsed;
}

void ringAdvance(ULONG& cursor, ULONG bytes) noexcept {
    cursor = (cursor + bytes) % kRingCapacityBytes;
}

void ringCopyIn(DEVICE_CONTEXT& ctx, const UCHAR* source, ULONG bytes) noexcept {
    if (bytes == 0 || source == nullptr || ctx.ringBuffer == nullptr) return;

    // Transport is frame-based. Do not ever leave a partial float32 stereo frame
    // in the cable, even if a malformed packet makes it this far.
    bytes -= bytes % kBlockAlign;
    if (bytes == 0) return;

    if (bytes >= kRingCapacityBytes) {
        source += bytes - kRingCapacityBytes;
        bytes = kRingCapacityBytes;
        ctx.ringRead = 0;
        ctx.ringWrite = 0;
        ctx.ringUsed = 0;
    }

    const ULONG freeBytes = ringFreeBytes(ctx);
    if (bytes > freeBytes) {
        ULONG discard = bytes - freeBytes;
        discard += (kBlockAlign - (discard % kBlockAlign)) % kBlockAlign;
        if (discard > ctx.ringUsed) discard = ctx.ringUsed;
        ringAdvance(ctx.ringRead, discard);
        ctx.ringUsed -= discard;
    }

    const ULONG first = minUlong(bytes, kRingCapacityBytes - ctx.ringWrite);
    RtlCopyMemory(ctx.ringBuffer + ctx.ringWrite, source, first);
    if (first < bytes) {
        RtlCopyMemory(ctx.ringBuffer, source + first, bytes - first);
    }
    ringAdvance(ctx.ringWrite, bytes);
    ctx.ringUsed += bytes;
}

void ringCopyOut(DEVICE_CONTEXT& ctx, UCHAR* destination, ULONG bytes) noexcept {
    if (bytes == 0 || destination == nullptr) return;

    const ULONG alignedBytes = bytes - (bytes % kBlockAlign);
    ULONG copied = 0;
    if (ctx.ringBuffer != nullptr && alignedBytes != 0) {
        copied = minUlong(alignedBytes, ctx.ringUsed);
        copied -= copied % kBlockAlign;

        const ULONG first = minUlong(copied, kRingCapacityBytes - ctx.ringRead);
        if (first != 0) RtlCopyMemory(destination, ctx.ringBuffer + ctx.ringRead, first);
        if (first < copied) RtlCopyMemory(destination + first, ctx.ringBuffer, copied - first);
        ringAdvance(ctx.ringRead, copied);
        ctx.ringUsed -= copied;
    }

    if (copied < bytes) RtlZeroMemory(destination + copied, bytes - copied);
}

void writeTransportChunk(
    DEVICE_CONTEXT& ctx,
    const UCHAR* data,
    ULONG dataBytes,
    ULONG totalBytes) noexcept {
    static const UCHAR silence[kTimerQuantumBytes]{};
    dataBytes = minUlong(dataBytes, totalBytes);
    dataBytes -= dataBytes % kBlockAlign;
    totalBytes -= totalBytes % kBlockAlign;

    KIRQL oldIrql{};
    KeAcquireSpinLock(&ctx.ringLock, &oldIrql);
    if (dataBytes != 0 && data != nullptr) ringCopyIn(ctx, data, dataBytes);
    if (dataBytes < totalBytes) ringCopyIn(ctx, silence, totalBytes - dataBytes);
    KeReleaseSpinLock(&ctx.ringLock, oldIrql);
}

void readMicrophone(DEVICE_CONTEXT& ctx, UCHAR* data, ULONG bytes) noexcept {
    KIRQL oldIrql{};
    KeAcquireSpinLock(&ctx.ringLock, &oldIrql);
    ringCopyOut(ctx, data, bytes);
    KeReleaseSpinLock(&ctx.ringLock, oldIrql);
}

void flushCable(DEVICE_CONTEXT& ctx) noexcept {
    KIRQL oldIrql{};
    KeAcquireSpinLock(&ctx.ringLock, &oldIrql);
    ctx.ringRead = 0;
    ctx.ringWrite = 0;
    ctx.ringUsed = 0;
    if (ctx.ringBuffer != nullptr) RtlZeroMemory(ctx.ringBuffer, kRingCapacityBytes);
    KeReleaseSpinLock(&ctx.ringLock, oldIrql);
}

[[nodiscard]] NTSTATUS validateStreamFormat(ACXDATAFORMAT streamFormat) noexcept {
    if (streamFormat == nullptr) return STATUS_INVALID_PARAMETER;
    const auto* wave = AcxDataFormatGetWaveFormatExtensible(streamFormat);
    if (wave == nullptr) return STATUS_INVALID_PARAMETER;

    if (wave->Format.nSamplesPerSec != kSampleRate ||
        wave->Format.nChannels != kChannels ||
        wave->Format.wBitsPerSample != kBitsPerSample ||
        wave->Format.nBlockAlign != kBlockAlign ||
        wave->Format.nAvgBytesPerSec != kAverageBytesPerSecond ||
        !IsEqualGUID(wave->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
        return STATUS_NOT_SUPPORTED;
    }
    return STATUS_SUCCESS;
}

[[nodiscard]] NTSTATUS addFixedFormat(WDFDEVICE device, ACXCIRCUIT circuit, ACXPIN hostPin) {
    ACX_DATAFORMAT_CONFIG formatConfig;
    ACX_DATAFORMAT_CONFIG_INIT_KS(&formatConfig, &kPuffyFloat48Stereo.DataFormat);

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = circuit;

    ACXDATAFORMAT format = nullptr;
    NTSTATUS status = AcxDataFormatCreate(device, &attributes, &formatConfig, &format);
    if (!NT_SUCCESS(status)) return status;

    ACXDATAFORMATLIST formatList = AcxPinGetRawDataFormatList(hostPin);
    if (formatList == nullptr) return STATUS_INVALID_DEVICE_STATE;

    return AcxDataFormatListAssignDefaultDataFormat(formatList, format);
}

[[nodiscard]] NTSTATUS createCircuit(WDFDEVICE device, BOOLEAN capture, ACXCIRCUIT* result) {
    if (result == nullptr) return STATUS_INVALID_PARAMETER;
    *result = nullptr;

    PACXCIRCUIT_INIT circuitInit = AcxCircuitInitAllocate(device);
    if (circuitInit == nullptr) return STATUS_INSUFFICIENT_RESOURCES;

    UNICODE_STRING circuitName{};
    RtlInitUnicodeString(&circuitName, capture ? kMicrophoneCircuitName : kTransportCircuitName);
    NTSTATUS status = AcxCircuitInitAssignName(circuitInit, &circuitName);
    if (!NT_SUCCESS(status)) {
        AcxCircuitInitFree(circuitInit);
        return status;
    }

    AcxCircuitInitSetCircuitType(circuitInit, capture ? AcxCircuitTypeCapture : AcxCircuitTypeRender);
    AcxCircuitInitSetComponentId(circuitInit,
        capture ? &GUID_PUFFY_MICROPHONE_COMPONENT : &GUID_PUFFY_TRANSPORT_COMPONENT);

    status = AcxCircuitInitAssignAcxCreateStreamCallback(
        circuitInit,
        capture ? PuffyEvtMicrophoneCreateStream : PuffyEvtTransportCreateStream);
    if (!NT_SUCCESS(status)) {
        AcxCircuitInitFree(circuitInit);
        return status;
    }

    WDF_OBJECT_ATTRIBUTES circuitAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&circuitAttributes);

    ACXCIRCUIT circuit = nullptr;
    status = AcxCircuitCreate(device, &circuitAttributes, &circuitInit, &circuit);
    if (!NT_SUCCESS(status)) {
        if (circuitInit != nullptr) AcxCircuitInitFree(circuitInit);
        return status;
    }

    ACX_PIN_CONFIG hostPinConfig;
    ACX_PIN_CONFIG_INIT(&hostPinConfig);
    hostPinConfig.Type = capture ? AcxPinTypeSource : AcxPinTypeSink;
    hostPinConfig.Communication = AcxPinCommunicationSink;
    hostPinConfig.Category = &KSCATEGORY_AUDIO;
    hostPinConfig.MaxStreams = 1;

    ACXPIN hostPin = nullptr;
    WDF_OBJECT_ATTRIBUTES pinAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&pinAttributes);
    pinAttributes.ParentObject = circuit;
    status = AcxPinCreate(circuit, &pinAttributes, &hostPinConfig, &hostPin);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(circuit);
        return status;
    }

    ACX_PIN_CONFIG bridgePinConfig;
    ACX_PIN_CONFIG_INIT(&bridgePinConfig);
    bridgePinConfig.Type = capture ? AcxPinTypeSink : AcxPinTypeSource;
    bridgePinConfig.Communication = AcxPinCommunicationNone;
    bridgePinConfig.Category = capture ? &KSNODETYPE_MICROPHONE : &KSNODETYPE_SPEAKER;

    ACXPIN bridgePin = nullptr;
    WDF_OBJECT_ATTRIBUTES_INIT(&pinAttributes);
    pinAttributes.ParentObject = circuit;
    status = AcxPinCreate(circuit, &pinAttributes, &bridgePinConfig, &bridgePin);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(circuit);
        return status;
    }

    status = addFixedFormat(device, circuit, hostPin);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(circuit);
        return status;
    }

    ACXPIN pins[] = {hostPin, bridgePin};
    status = AcxCircuitAddPins(circuit, pins, ARRAYSIZE(pins));
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(circuit);
        return status;
    }

    *result = circuit;
    return STATUS_SUCCESS;
}

[[nodiscard]] ULONGLONG streamTimeHns(const STREAM_CONTEXT& ctx) noexcept {
    const LARGE_INTEGER qpc = KeQueryPerformanceCounter(nullptr);
    return qpcToHns(qpc.QuadPart, ctx.performanceCounterFrequency.QuadPart);
}

void publishPresentation(STREAM_CONTEXT& ctx, ULONGLONG positionBytes, LONGLONG qpc) noexcept {
    // A tiny seqlock keeps the position and the QPC snapshot from being mixed
    // across two timer updates without putting a lock in the WaveRT query path.
    InterlockedIncrement(&ctx.presentationSequence); // odd: writer active
    InterlockedExchange64(&ctx.presentationPositionBytes, static_cast<LONG64>(positionBytes));
    InterlockedExchange64(&ctx.presentationQpc, qpc);
    InterlockedIncrement(&ctx.presentationSequence); // even: stable pair
}

void resetRenderPacketState(STREAM_CONTEXT& ctx) noexcept {
    for (ULONG i = 0; i < ARRAYSIZE(ctx.renderReadyValid); ++i) {
        InterlockedExchange(&ctx.renderReadyValid[i], FALSE);
        InterlockedExchange(&ctx.renderReadyPacket[i], 0);
        InterlockedExchange(&ctx.renderReadyBytes[i], 0);
    }
    InterlockedExchange(&ctx.eosReceived, FALSE);
}

[[nodiscard]] LONG packetDistance(ULONG packet, ULONG current) noexcept {
    // Packet counters are ULONGs and are allowed to wrap. Interpreting the
    // modulo subtraction as LONG gives the usual before/current/after ordering
    // as long as callers stay within the tiny (one-packet) WaveRT window.
    return static_cast<LONG>(packet - current);
}

[[nodiscard]] bool renderPacketReady(
    STREAM_CONTEXT& ctx,
    ULONG packet,
    ULONG* validBytes) noexcept {
    if (validBytes == nullptr || ctx.packetCount == 0) return false;
    const ULONG slot = packet % ctx.packetCount;
    if (slot >= ARRAYSIZE(ctx.renderReadyValid)) return false;

    // Interlocked operations provide the ordering barrier paired with the
    // writer in SetRenderPacket (identity/length first, valid flag last).
    if (InterlockedCompareExchange(&ctx.renderReadyValid[slot], FALSE, FALSE) != TRUE) return false;
    const ULONG readyPacket = static_cast<ULONG>(
        InterlockedCompareExchange(&ctx.renderReadyPacket[slot], 0, 0));
    if (readyPacket != packet) return false;

    LONG readyBytes = InterlockedCompareExchange(&ctx.renderReadyBytes[slot], 0, 0);
    if (readyBytes < 0) return false;
    *validBytes = minUlong(static_cast<ULONG>(readyBytes), ctx.packetSize);
    return true;
}

void retireRenderPacket(STREAM_CONTEXT& ctx, ULONG packet) noexcept {
    if (ctx.packetCount == 0) return;
    const ULONG slot = packet % ctx.packetCount;
    if (slot >= ARRAYSIZE(ctx.renderReadyValid)) return;

    const ULONG readyPacket = static_cast<ULONG>(
        InterlockedCompareExchange(&ctx.renderReadyPacket[slot], 0, 0));
    if (readyPacket == packet) {
        InterlockedExchange(&ctx.renderReadyValid[slot], FALSE);
    }
}

[[nodiscard]] ULONG nextTransferBytes(const STREAM_CONTEXT& ctx, ULONGLONG processedBytes) noexcept {
    if (ctx.packetSize == 0) return 0;
    const ULONG offset = static_cast<ULONG>(processedBytes % ctx.packetSize);
    const ULONG remaining = ctx.packetSize - offset;
    return minUlong(kTimerQuantumBytes, remaining);
}

void scheduleNextTransfer(STREAM_CONTEXT& ctx) noexcept {
    if (ctx.packetTimer == nullptr ||
        InterlockedCompareExchange(&ctx.running, TRUE, TRUE) != TRUE ||
        ctx.bytesPerSecond == 0 || ctx.packetSize == 0) {
        return;
    }

    const ULONGLONG processed = static_cast<ULONGLONG>(
        InterlockedCompareExchange64(&ctx.processedBytes, 0, 0));
    const ULONG transferBytes = nextTransferBytes(ctx, processed);
    if (transferBytes == 0) return;

    const ULONGLONG nextPosition = processed + transferBytes;
    const ULONGLONG runStartPosition = static_cast<ULONGLONG>(
        InterlockedCompareExchange64(&ctx.runStartPositionBytes, 0, 0));
    const ULONGLONG bytesFromRun = nextPosition > runStartPosition
        ? nextPosition - runStartPosition
        : transferBytes;

    const ULONGLONG runStartHns = static_cast<ULONGLONG>(
        InterlockedCompareExchange64(&ctx.runStartTimeHns, 0, 0));
    ULONGLONG glitchAdjustment = static_cast<ULONGLONG>(
        InterlockedCompareExchange64(&ctx.glitchAdjustmentHns, 0, 0));
    ULONGLONG targetHns = runStartHns + glitchAdjustment +
        bytesToHns(bytesFromRun, ctx.bytesPerSecond);
    const ULONGLONG nowHns = streamTimeHns(ctx);

    // Do not fire a large burst of stale sub-packets after a scheduler stall,
    // sleep transition, or debugger break. Shift the virtual hardware clock
    // forward instead and continue at the nominal audio rate.
    if (targetHns <= nowHns) {
        const ULONGLONG correction = (nowHns - targetHns) + 1ULL;
        InterlockedAdd64(&ctx.glitchAdjustmentHns, static_cast<LONG64>(correction));
        glitchAdjustment += correction;
        targetHns += correction;
    }

    const ULONGLONG waitHns = targetHns > nowHns ? targetHns - nowHns : 1ULL;
    // WDF timer due times are negative for relative waits and are expressed in
    // 100-ns units, which is exactly the HNS unit calculated above.
    (void)WdfTimerStart(ctx.packetTimer, -static_cast<LONGLONG>(waitHns));
}

[[nodiscard]] NTSTATUS configureStream(
    WDFDEVICE device,
    ACXCIRCUIT circuit,
    PACXSTREAM_INIT streamInit,
    ACXDATAFORMAT streamFormat,
    BOOLEAN capture) {

    NTSTATUS status = validateStreamFormat(streamFormat);
    if (!NT_SUCCESS(status)) return status;

    ACX_STREAM_CALLBACKS streamCallbacks;
    ACX_STREAM_CALLBACKS_INIT(&streamCallbacks);
    streamCallbacks.EvtAcxStreamPrepareHardware = PuffyEvtStreamPrepareHardware;
    streamCallbacks.EvtAcxStreamReleaseHardware = PuffyEvtStreamReleaseHardware;
    streamCallbacks.EvtAcxStreamRun = PuffyEvtStreamRun;
    streamCallbacks.EvtAcxStreamPause = PuffyEvtStreamPause;
    status = AcxStreamInitAssignAcxStreamCallbacks(streamInit, &streamCallbacks);
    if (!NT_SUCCESS(status)) return status;

    ACX_RT_STREAM_CALLBACKS rtCallbacks;
    ACX_RT_STREAM_CALLBACKS_INIT(&rtCallbacks);
    rtCallbacks.EvtAcxStreamAllocateRtPackets = PuffyEvtStreamAllocateRtPackets;
    rtCallbacks.EvtAcxStreamFreeRtPackets = PuffyEvtStreamFreeRtPackets;
    rtCallbacks.EvtAcxStreamGetHwLatency = PuffyEvtStreamGetHwLatency;
    rtCallbacks.EvtAcxStreamGetCurrentPacket = PuffyEvtStreamGetCurrentPacket;
    rtCallbacks.EvtAcxStreamGetPresentationPosition = PuffyEvtStreamGetPresentationPosition;
    if (capture) {
        rtCallbacks.EvtAcxStreamGetCapturePacket = PuffyEvtStreamGetCapturePacket;
    } else {
        rtCallbacks.EvtAcxStreamSetRenderPacket = PuffyEvtStreamSetRenderPacket;
    }
    status = AcxStreamInitAssignAcxRtStreamCallbacks(streamInit, &rtCallbacks);
    if (!NT_SUCCESS(status)) return status;

    AcxStreamInitSetAcxRtStreamSupportsNotifications(streamInit);

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, STREAM_CONTEXT);
    attributes.EvtCleanupCallback = PuffyEvtStreamCleanup;

    ACXSTREAM stream = nullptr;
    status = AcxRtStreamCreate(device, circuit, &attributes, &streamInit, &stream);
    if (!NT_SUCCESS(status)) return status;

    auto* ctx = GetPuffyStreamContext(stream);
    ctx->stream = stream;
    ctx->deviceContext = GetPuffyDeviceContext(device);
    ctx->capture = capture;
    ctx->blockAlign = kBlockAlign;
    ctx->bytesPerSecond = kAverageBytesPerSecond;
    ctx->currentPacket = 0;
    ctx->processedBytes = 0;
    ctx->currentPacketStartQpc = 0;
    ctx->lastPacketStartQpc = 0;
    const LARGE_INTEGER initialQpc = KeQueryPerformanceCounter(&ctx->performanceCounterFrequency);
    ctx->presentationSequence = 0;
    ctx->presentationPositionBytes = 0;
    ctx->presentationQpc = initialQpc.QuadPart;
    ctx->runStartTimeHns = 0;
    ctx->runStartPositionBytes = 0;
    ctx->glitchAdjustmentHns = 0;
    resetRenderPacketState(*ctx);
    return STATUS_SUCCESS;
}

void freePackets(STREAM_CONTEXT& ctx) noexcept {
    for (ULONG i = 0; i < ARRAYSIZE(ctx.packetBuffers); ++i) {
        if (ctx.packetMdls[i] != nullptr) {
            IoFreeMdl(ctx.packetMdls[i]);
            ctx.packetMdls[i] = nullptr;
        }
        if (ctx.packetBuffers[i] != nullptr) {
            ExFreePoolWithTag(ctx.packetBuffers[i], kPoolTag);
            ctx.packetBuffers[i] = nullptr;
        }
    }
    if (ctx.packets != nullptr) {
        ExFreePoolWithTag(ctx.packets, kPoolTag);
        ctx.packets = nullptr;
    }
    ctx.packetCount = 0;
    ctx.packetSize = 0;
    resetRenderPacketState(ctx);
}

} // namespace

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driverObject, PUNICODE_STRING registryPath) {
    WDF_DRIVER_CONFIG wdfConfig;
    WDF_DRIVER_CONFIG_INIT(&wdfConfig, PuffyEvtDeviceAdd);

    WDFDRIVER driver = nullptr;
    NTSTATUS status = WdfDriverCreate(driverObject, registryPath, WDF_NO_OBJECT_ATTRIBUTES, &wdfConfig, &driver);
    if (!NT_SUCCESS(status)) return status;

    ACX_DRIVER_CONFIG acxConfig;
    ACX_DRIVER_CONFIG_INIT(&acxConfig);
    return AcxDriverInitialize(driver, &acxConfig);
}

NTSTATUS PuffyEvtDeviceAdd(WDFDRIVER driver, PWDFDEVICE_INIT deviceInit) {
    UNREFERENCED_PARAMETER(driver);

    // ACX must initialize the WDFDEVICE_INIT before WdfDeviceCreate so the
    // framework can apply the audio-class defaults to this root-enumerated FDO.
    ACX_DEVICEINIT_CONFIG deviceInitConfig;
    ACX_DEVICEINIT_CONFIG_INIT(&deviceInitConfig);
    NTSTATUS status = AcxDeviceInitInitialize(deviceInit, &deviceInitConfig);
    if (!NT_SUCCESS(status)) return status;

    WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);
    pnpCallbacks.EvtDevicePrepareHardware = PuffyEvtDevicePrepareHardware;
    pnpCallbacks.EvtDeviceReleaseHardware = PuffyEvtDeviceReleaseHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(deviceInit, &pnpCallbacks);

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);
    attributes.EvtCleanupCallback = PuffyEvtDeviceCleanup;

    WDFDEVICE device = nullptr;
    status = WdfDeviceCreate(&deviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) return status;

    ACX_DEVICE_CONFIG deviceConfig;
    ACX_DEVICE_CONFIG_INIT(&deviceConfig);
    status = AcxDeviceInitialize(device, &deviceConfig);
    if (!NT_SUCCESS(status)) return status;

    auto* ctx = GetPuffyDeviceContext(device);
    KeInitializeSpinLock(&ctx->ringLock);
    ctx->ringBuffer = static_cast<PUCHAR>(
        ExAllocatePool2(POOL_FLAG_NON_PAGED, kRingCapacityBytes, kPoolTag));
    if (ctx->ringBuffer == nullptr) return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(ctx->ringBuffer, kRingCapacityBytes);

    status = createCircuit(device, FALSE, &ctx->transportCircuit);
    if (!NT_SUCCESS(status)) return status;

    status = createCircuit(device, TRUE, &ctx->microphoneCircuit);
    if (!NT_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

void PuffyEvtDeviceCleanup(WDFOBJECT object) {
    auto* ctx = GetPuffyDeviceContext(object);
    if (ctx->ringBuffer != nullptr) {
        ExFreePoolWithTag(ctx->ringBuffer, kPoolTag);
        ctx->ringBuffer = nullptr;
    }
}

NTSTATUS PuffyEvtDevicePrepareHardware(WDFDEVICE device, WDFCMRESLIST rawResources, WDFCMRESLIST translatedResources) {
    UNREFERENCED_PARAMETER(rawResources);
    UNREFERENCED_PARAMETER(translatedResources);

    auto* ctx = GetPuffyDeviceContext(device);
    if (ctx->circuitsAdded) return STATUS_SUCCESS;

    flushCable(*ctx);
    NTSTATUS status = AcxDeviceAddCircuit(device, ctx->transportCircuit);
    if (!NT_SUCCESS(status)) return status;

    status = AcxDeviceAddCircuit(device, ctx->microphoneCircuit);
    if (!NT_SUCCESS(status)) {
        (void)AcxDeviceRemoveCircuit(device, ctx->transportCircuit);
        return status;
    }

    ctx->circuitsAdded = TRUE;
    return STATUS_SUCCESS;
}

NTSTATUS PuffyEvtDeviceReleaseHardware(WDFDEVICE device, WDFCMRESLIST translatedResources) {
    UNREFERENCED_PARAMETER(translatedResources);
    auto* ctx = GetPuffyDeviceContext(device);
    if (!ctx->circuitsAdded) return STATUS_SUCCESS;

    NTSTATUS microphoneStatus = AcxDeviceRemoveCircuit(device, ctx->microphoneCircuit);
    NTSTATUS transportStatus = AcxDeviceRemoveCircuit(device, ctx->transportCircuit);
    ctx->circuitsAdded = FALSE;
    flushCable(*ctx);

    return NT_SUCCESS(microphoneStatus) ? transportStatus : microphoneStatus;
}

NTSTATUS PuffyEvtTransportCreateStream(
    WDFDEVICE device,
    ACXCIRCUIT circuit,
    ACXPIN pin,
    PACXSTREAM_INIT streamInit,
    ACXDATAFORMAT streamFormat,
    const GUID* signalProcessingMode,
    ACXOBJECTBAG varArguments) {
    UNREFERENCED_PARAMETER(pin);
    UNREFERENCED_PARAMETER(signalProcessingMode);
    UNREFERENCED_PARAMETER(varArguments);
    return configureStream(device, circuit, streamInit, streamFormat, FALSE);
}

NTSTATUS PuffyEvtMicrophoneCreateStream(
    WDFDEVICE device,
    ACXCIRCUIT circuit,
    ACXPIN pin,
    PACXSTREAM_INIT streamInit,
    ACXDATAFORMAT streamFormat,
    const GUID* signalProcessingMode,
    ACXOBJECTBAG varArguments) {
    UNREFERENCED_PARAMETER(pin);
    UNREFERENCED_PARAMETER(signalProcessingMode);
    UNREFERENCED_PARAMETER(varArguments);
    return configureStream(device, circuit, streamInit, streamFormat, TRUE);
}

NTSTATUS PuffyEvtStreamAllocateRtPackets(ACXSTREAM stream, ULONG packetCount, ULONG packetSize, PACX_RTPACKET* packets) {
    if (packets == nullptr || packetCount == 0 || packetCount > 2 || packetSize == 0 ||
        (packetSize % kBlockAlign) != 0) {
        return STATUS_INVALID_PARAMETER;
    }

    auto* ctx = GetPuffyStreamContext(stream);
    if (ctx->packets != nullptr) return STATUS_INVALID_DEVICE_STATE;

    SIZE_T descriptorsBytes = 0;
    NTSTATUS status = RtlSizeTMult(
        static_cast<SIZE_T>(packetCount),
        sizeof(ACX_RTPACKET),
        &descriptorsBytes);
    if (!NT_SUCCESS(status)) return status;

    ULONG roundedPacketBytes = 0;
    status = RtlULongAdd(packetSize, PAGE_SIZE - 1, &roundedPacketBytes);
    if (!NT_SUCCESS(status)) return status;
    const ULONG allocationBytes = (roundedPacketBytes / PAGE_SIZE) * PAGE_SIZE;
    if (allocationBytes < packetSize || allocationBytes == 0) return STATUS_INTEGER_OVERFLOW;
    const ULONG packetOffset = allocationBytes - packetSize;

    auto* descriptors = static_cast<PACX_RTPACKET>(
        ExAllocatePool2(POOL_FLAG_NON_PAGED, descriptorsBytes, kPoolTag));
    if (descriptors == nullptr) return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(descriptors, descriptorsBytes);

    for (ULONG i = 0; i < packetCount; ++i) {
        ACX_RTPACKET_INIT(&descriptors[i]);

        PVOID packetBuffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, allocationBytes, kPoolTag);
        if (packetBuffer == nullptr) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }
        RtlZeroMemory(packetBuffer, allocationBytes);

        PMDL mdl = IoAllocateMdl(packetBuffer, allocationBytes, FALSE, TRUE, nullptr);
        if (mdl == nullptr) {
            ExFreePoolWithTag(packetBuffer, kPoolTag);
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }
        MmBuildMdlForNonPagedPool(mdl);

        WDF_MEMORY_DESCRIPTOR_INIT_MDL(&descriptors[i].RtPacketBuffer, mdl, allocationBytes);
        descriptors[i].RtPacketOffset = (i == 0) ? packetOffset : 0;
        descriptors[i].RtPacketSize = packetSize;
        ctx->packetBuffers[i] = packetBuffer;
        ctx->packetMdls[i] = mdl;
    }

    if (!NT_SUCCESS(status)) {
        ctx->packets = descriptors;
        freePackets(*ctx);
        return status;
    }

    ctx->packets = descriptors;
    ctx->packetCount = packetCount;
    ctx->packetSize = packetSize;
    InterlockedExchange(&ctx->currentPacket, 0);
    InterlockedExchange64(&ctx->processedBytes, 0);
    InterlockedExchange64(&ctx->currentPacketStartQpc, 0);
    InterlockedExchange64(&ctx->lastPacketStartQpc, 0);
    resetRenderPacketState(*ctx);
    const LARGE_INTEGER qpc = KeQueryPerformanceCounter(nullptr);
    publishPresentation(*ctx, 0, qpc.QuadPart);
    *packets = descriptors;
    return STATUS_SUCCESS;
}

void PuffyEvtStreamFreeRtPackets(ACXSTREAM stream, PACX_RTPACKET packets, ULONG packetCount) {
    UNREFERENCED_PARAMETER(packetCount);
    auto* ctx = GetPuffyStreamContext(stream);
    if (packets == ctx->packets) freePackets(*ctx);
}

NTSTATUS PuffyEvtStreamPrepareHardware(ACXSTREAM stream) {
    auto* ctx = GetPuffyStreamContext(stream);
    if (ctx->packetTimer == nullptr) {
        WDF_TIMER_CONFIG timerConfig;
        WDF_TIMER_CONFIG_INIT(&timerConfig, PuffyEvtPacketTimer);
        timerConfig.AutomaticSerialization = FALSE;
        timerConfig.UseHighResolutionTimer = WdfTrue;

        WDF_OBJECT_ATTRIBUTES attributes;
        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
        attributes.ParentObject = stream;

        NTSTATUS status = WdfTimerCreate(&timerConfig, &attributes, &ctx->packetTimer);
        if (!NT_SUCCESS(status)) return status;
    }

    ctx->prepared = TRUE;
    return STATUS_SUCCESS;
}

NTSTATUS PuffyEvtStreamReleaseHardware(ACXSTREAM stream) {
    auto* ctx = GetPuffyStreamContext(stream);
    InterlockedExchange(&ctx->running, FALSE);
    if (ctx->packetTimer != nullptr) WdfTimerStop(ctx->packetTimer, TRUE);
    ctx->prepared = FALSE;
    InterlockedExchange(&ctx->currentPacket, 0);
    InterlockedExchange64(&ctx->processedBytes, 0);
    InterlockedExchange64(&ctx->currentPacketStartQpc, 0);
    InterlockedExchange64(&ctx->lastPacketStartQpc, 0);
    InterlockedExchange64(&ctx->runStartTimeHns, 0);
    InterlockedExchange64(&ctx->runStartPositionBytes, 0);
    InterlockedExchange64(&ctx->glitchAdjustmentHns, 0);
    resetRenderPacketState(*ctx);
    const LARGE_INTEGER qpc = KeQueryPerformanceCounter(nullptr);
    publishPresentation(*ctx, 0, qpc.QuadPart);
    if (ctx->deviceContext != nullptr) flushCable(*ctx->deviceContext);
    return STATUS_SUCCESS;
}

NTSTATUS PuffyEvtStreamRun(ACXSTREAM stream) {
    auto* ctx = GetPuffyStreamContext(stream);
    if (!ctx->prepared || ctx->packetTimer == nullptr || ctx->packets == nullptr) return STATUS_INVALID_DEVICE_STATE;

    LARGE_INTEGER frequency{};
    const LARGE_INTEGER qpc = KeQueryPerformanceCounter(&frequency);
    ctx->performanceCounterFrequency = frequency;
    const LONG64 processed = InterlockedCompareExchange64(&ctx->processedBytes, 0, 0);
    if ((static_cast<ULONGLONG>(processed) % ctx->packetSize) == 0 ||
        InterlockedCompareExchange64(&ctx->currentPacketStartQpc, 0, 0) == 0) {
        InterlockedExchange64(&ctx->currentPacketStartQpc, qpc.QuadPart);
    }
    InterlockedExchange64(&ctx->runStartTimeHns, static_cast<LONG64>(qpcToHns(qpc.QuadPart, frequency.QuadPart)));
    InterlockedExchange64(&ctx->runStartPositionBytes, processed);
    InterlockedExchange64(&ctx->glitchAdjustmentHns, 0);
    publishPresentation(*ctx, static_cast<ULONGLONG>(processed), qpc.QuadPart);

    // Starting either side defines a fresh cable session. This prevents a
    // capture client opened later from receiving up to 200 ms of old audio.
    if (ctx->deviceContext != nullptr) flushCable(*ctx->deviceContext);
    InterlockedExchange(&ctx->running, TRUE);
    scheduleNextTransfer(*ctx);
    return STATUS_SUCCESS;
}

NTSTATUS PuffyEvtStreamPause(ACXSTREAM stream) {
    auto* ctx = GetPuffyStreamContext(stream);
    InterlockedExchange(&ctx->running, FALSE);
    if (ctx->packetTimer != nullptr) WdfTimerStop(ctx->packetTimer, TRUE);
    return STATUS_SUCCESS;
}

void PuffyEvtPacketTimer(WDFTIMER timer) {
    ACXSTREAM stream = reinterpret_cast<ACXSTREAM>(WdfTimerGetParentObject(timer));
    auto* ctx = GetPuffyStreamContext(stream);
    if (InterlockedCompareExchange(&ctx->running, TRUE, TRUE) != TRUE ||
        ctx->packets == nullptr || ctx->packetCount == 0 || ctx->packetSize == 0 ||
        ctx->deviceContext == nullptr) {
        return;
    }

    const ULONGLONG processedBefore = static_cast<ULONGLONG>(
        InterlockedCompareExchange64(&ctx->processedBytes, 0, 0));
    const ULONG packetOffset = static_cast<ULONG>(processedBefore % ctx->packetSize);
    const ULONG transferBytes = nextTransferBytes(*ctx, processedBefore);
    if (transferBytes == 0) return;

    const ULONG currentPacket = static_cast<ULONG>(
        InterlockedCompareExchange(&ctx->currentPacket, 0, 0));
    const ULONG packetIndex = currentPacket % ctx->packetCount;
    UCHAR* packetData = static_cast<UCHAR*>(ctx->packetBuffers[packetIndex]) +
        ctx->packets[packetIndex].RtPacketOffset + packetOffset;

    // Move only the bytes represented by this 2-ms presentation-position step.
    // For render, never touch an AudioKSE-owned physical slot until
    // SetRenderPacket has released this exact logical packet. Missing/late
    // render data advances as silence so a stale reused buffer can never leak
    // into the virtual microphone.
    if (ctx->capture) {
        readMicrophone(*ctx->deviceContext, packetData, transferBytes);
    } else {
        ULONG validPacketBytes = 0;
        ULONG dataBytes = 0;
        if (renderPacketReady(*ctx, currentPacket, &validPacketBytes) &&
            packetOffset < validPacketBytes) {
            dataBytes = minUlong(transferBytes, validPacketBytes - packetOffset);
            dataBytes -= dataBytes % kBlockAlign;
        }
        writeTransportChunk(*ctx->deviceContext, packetData, dataBytes, transferBytes);
    }

    const LARGE_INTEGER qpc = KeQueryPerformanceCounter(nullptr);
    const ULONGLONG processedAfter = static_cast<ULONGLONG>(
        InterlockedAdd64(&ctx->processedBytes, transferBytes));
    publishPresentation(*ctx, processedAfter, qpc.QuadPart);

    if (packetOffset + transferBytes == ctx->packetSize) {
        const LONG64 packetStartQpc = InterlockedExchange64(&ctx->currentPacketStartQpc, qpc.QuadPart);
        InterlockedExchange64(&ctx->lastPacketStartQpc, packetStartQpc);
        if (!ctx->capture) retireRenderPacket(*ctx, currentPacket);
        InterlockedIncrement(&ctx->currentPacket);
        (void)AcxRtStreamNotifyPacketComplete(stream, currentPacket, qpc.QuadPart);
    }

    scheduleNextTransfer(*ctx);
}

NTSTATUS PuffyEvtStreamGetHwLatency(ACXSTREAM stream, PULONG fifoSize, PULONG delay) {
    UNREFERENCED_PARAMETER(stream);
    if (fifoSize == nullptr || delay == nullptr) return STATUS_INVALID_PARAMETER;
    *fifoSize = 0;
    *delay = 0;
    return STATUS_SUCCESS;
}

NTSTATUS PuffyEvtStreamGetCurrentPacket(ACXSTREAM stream, PULONG currentPacket) {
    if (currentPacket == nullptr) return STATUS_INVALID_PARAMETER;
    auto* ctx = GetPuffyStreamContext(stream);
    *currentPacket = static_cast<ULONG>(InterlockedCompareExchange(&ctx->currentPacket, 0, 0));
    return STATUS_SUCCESS;
}

NTSTATUS PuffyEvtStreamGetPresentationPosition(ACXSTREAM stream, PULONGLONG positionInBlocks, PULONGLONG qpcPosition) {
    if (positionInBlocks == nullptr || qpcPosition == nullptr) return STATUS_INVALID_PARAMETER;
    auto* ctx = GetPuffyStreamContext(stream);

    LONG before = 0;
    LONG after = 0;
    LONG64 bytes = 0;
    LONG64 qpc = 0;
    do {
        before = InterlockedCompareExchange(&ctx->presentationSequence, 0, 0);
        if ((before & 1) != 0) continue;
        bytes = InterlockedCompareExchange64(&ctx->presentationPositionBytes, 0, 0);
        qpc = InterlockedCompareExchange64(&ctx->presentationQpc, 0, 0);
        after = InterlockedCompareExchange(&ctx->presentationSequence, 0, 0);
    } while (before != after || (after & 1) != 0);

    *positionInBlocks = static_cast<ULONGLONG>(bytes) / nonZeroUlong(ctx->blockAlign);
    *qpcPosition = static_cast<ULONGLONG>(qpc);
    return STATUS_SUCCESS;
}

NTSTATUS PuffyEvtStreamSetRenderPacket(ACXSTREAM stream, ULONG packet, ULONG flags, ULONG eosPacketLength) {
    if ((flags & ~KSSTREAM_HEADER_OPTIONSF_ENDOFSTREAM) != 0) return STATUS_INVALID_PARAMETER;

    auto* ctx = GetPuffyStreamContext(stream);
    if (ctx->capture || ctx->packetCount == 0 || ctx->packetSize == 0 || ctx->packets == nullptr) {
        return STATUS_INVALID_DEVICE_STATE;
    }
    if (InterlockedCompareExchange(&ctx->eosReceived, FALSE, FALSE) == TRUE) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    const ULONG current = static_cast<ULONG>(
        InterlockedCompareExchange(&ctx->currentPacket, 0, 0));
    const ULONGLONG processed = static_cast<ULONGLONG>(
        InterlockedCompareExchange64(&ctx->processedBytes, 0, 0));
    const ULONG currentOffset = static_cast<ULONG>(processed % ctx->packetSize);
    const LONG distance = packetDistance(packet, current);

    if (distance < 0 || (distance == 0 && currentOffset != 0)) {
        return STATUS_DATA_LATE_ERROR;
    }
    // With one or two ACX RT packets, AudioKSE can release at most the current
    // packet (during pre-roll / before transfer begins) or the immediately next
    // logical packet. Anything further ahead cannot fit without overwriting a
    // physical WaveRT slot that is still owned by the driver.
    if (distance > 1) return STATUS_DATA_OVERRUN;

    const ULONG slot = packet % ctx->packetCount;
    if (slot >= ARRAYSIZE(ctx->renderReadyValid)) return STATUS_INVALID_DEVICE_STATE;
    if (InterlockedCompareExchange(&ctx->renderReadyValid[slot], FALSE, FALSE) == TRUE) {
        const ULONG existing = static_cast<ULONG>(
            InterlockedCompareExchange(&ctx->renderReadyPacket[slot], 0, 0));
        if (existing == packet) return STATUS_DATA_LATE_ERROR;
        if (packetDistance(existing, current) >= 0) return STATUS_DATA_OVERRUN;
        // A stale identity from an already-retired packet is safe to replace.
    }

    ULONG validBytes = ctx->packetSize;
    if ((flags & KSSTREAM_HEADER_OPTIONSF_ENDOFSTREAM) != 0) {
        if (eosPacketLength > ctx->packetSize) {
            return STATUS_INVALID_PARAMETER;
        }
        validBytes = eosPacketLength;
    }

    // Publish identity and valid length before the valid flag. Interlocked
    // operations are full barriers on Windows, so the timer cannot observe a
    // ready slot with metadata from the previous physical-buffer generation.
    InterlockedExchange(&ctx->renderReadyPacket[slot], static_cast<LONG>(packet));
    InterlockedExchange(&ctx->renderReadyBytes[slot], static_cast<LONG>(validBytes));
    InterlockedExchange(&ctx->renderReadyValid[slot], TRUE);
    if ((flags & KSSTREAM_HEADER_OPTIONSF_ENDOFSTREAM) != 0) {
        InterlockedExchange(&ctx->eosReceived, TRUE);
    }
    return STATUS_SUCCESS;
}

NTSTATUS PuffyEvtStreamGetCapturePacket(
    ACXSTREAM stream,
    PULONG lastCapturePacket,
    PULONGLONG qpcPacketStart,
    PBOOLEAN moreData) {
    if (lastCapturePacket == nullptr || qpcPacketStart == nullptr || moreData == nullptr) return STATUS_INVALID_PARAMETER;

    auto* ctx = GetPuffyStreamContext(stream);
    const ULONG current = static_cast<ULONG>(InterlockedCompareExchange(&ctx->currentPacket, 0, 0));
    *lastCapturePacket = current - 1;
    *qpcPacketStart = static_cast<ULONGLONG>(InterlockedCompareExchange64(&ctx->lastPacketStartQpc, 0, 0));
    *moreData = FALSE;
    return STATUS_SUCCESS;
}

void PuffyEvtStreamCleanup(WDFOBJECT object) {
    ACXSTREAM stream = reinterpret_cast<ACXSTREAM>(object);
    auto* ctx = GetPuffyStreamContext(stream);
    InterlockedExchange(&ctx->running, FALSE);
    if (ctx->packetTimer != nullptr) WdfTimerStop(ctx->packetTimer, TRUE);
    freePackets(*ctx);
}

} // namespace puffy::virtual_audio::driver
