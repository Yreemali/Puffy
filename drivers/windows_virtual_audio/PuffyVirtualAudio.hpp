#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <acx.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmsystem.h>

namespace puffy::virtual_audio::driver {

inline constexpr ULONG kSampleRate = 48'000;
inline constexpr USHORT kChannels = 2;
inline constexpr USHORT kBitsPerSample = 32;
inline constexpr USHORT kBlockAlign = kChannels * (kBitsPerSample / 8);
inline constexpr ULONG kAverageBytesPerSecond = kSampleRate * kBlockAlign;
inline constexpr ULONG kRingMilliseconds = 200;
inline constexpr ULONG kRingCapacityBytes = (kAverageBytesPerSecond * kRingMilliseconds) / 1000;
inline constexpr ULONG kPoolTag = 'yffP';

// The names below are circuit reference strings. The user-visible endpoint
// names are assigned by PuffyVirtualAudio.inf.
inline constexpr wchar_t kTransportCircuitName[] = L"Transport0";
inline constexpr wchar_t kMicrophoneCircuitName[] = L"Microphone0";

struct DEVICE_CONTEXT {
    ACXCIRCUIT transportCircuit{nullptr};
    ACXCIRCUIT microphoneCircuit{nullptr};

    KSPIN_LOCK ringLock{};
    PUCHAR ringBuffer{nullptr};
    ULONG ringRead{0};
    ULONG ringWrite{0};
    ULONG ringUsed{0};
    BOOLEAN circuitsAdded{FALSE};
};
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetPuffyDeviceContext);

struct STREAM_CONTEXT {
    ACXSTREAM stream{nullptr};
    DEVICE_CONTEXT* deviceContext{nullptr};
    BOOLEAN capture{FALSE};
    BOOLEAN prepared{FALSE};
    volatile LONG running{FALSE};

    WDFTIMER packetTimer{nullptr};

    PACX_RTPACKET packets{nullptr};
    ULONG packetCount{0};
    ULONG packetSize{0};
    PVOID packetBuffers[2]{nullptr, nullptr};
    PMDL packetMdls[2]{nullptr, nullptr};

    ULONG blockAlign{kBlockAlign};
    ULONG bytesPerSecond{kAverageBytesPerSecond};
    volatile LONG currentPacket{0};
    volatile LONG64 processedBytes{0};
    volatile LONG64 currentPacketStartQpc{0};
    volatile LONG64 lastPacketStartQpc{0};

    // Render WaveRT buffers belong to AudioKSE. A physical packet slot is
    // readable only after EvtAcxStreamSetRenderPacket has released that exact
    // logical packet. Keeping packet identity as well as a valid bit prevents
    // stale reads when one physical slot is reused by a later packet.
    volatile LONG renderReadyValid[2]{FALSE, FALSE};
    volatile LONG renderReadyPacket[2]{0, 0};
    volatile LONG renderReadyBytes[2]{0, 0};

    LARGE_INTEGER performanceCounterFrequency{};
    // Presentation position is published as a coherent (position,QPC) pair.
    // The packet timer advances it in small sub-packet chunks, so timer-driven
    // WaveRT clients never receive permission to overwrite bytes before this
    // driver has actually consumed them.
    volatile LONG presentationSequence{0};
    volatile LONG64 presentationPositionBytes{0};
    volatile LONG64 presentationQpc{0};
    volatile LONG64 runStartTimeHns{0};
    volatile LONG64 runStartPositionBytes{0};
    volatile LONG64 glitchAdjustmentHns{0};

    // ACX requires SetRenderPacket calls after EOS to fail until the stream is
    // stopped/released. Puffy normally renders a continuous stream, but keeping
    // the state makes the WaveRT contract correct for other clients too.
    volatile LONG eosReceived{FALSE};
};
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(STREAM_CONTEXT, GetPuffyStreamContext);

extern "C" DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD PuffyEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP PuffyEvtDeviceCleanup;
EVT_WDF_DEVICE_PREPARE_HARDWARE PuffyEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE PuffyEvtDeviceReleaseHardware;

EVT_ACX_CIRCUIT_CREATE_STREAM PuffyEvtTransportCreateStream;
EVT_ACX_CIRCUIT_CREATE_STREAM PuffyEvtMicrophoneCreateStream;

EVT_ACX_STREAM_PREPARE_HARDWARE PuffyEvtStreamPrepareHardware;
EVT_ACX_STREAM_RELEASE_HARDWARE PuffyEvtStreamReleaseHardware;
EVT_ACX_STREAM_RUN PuffyEvtStreamRun;
EVT_ACX_STREAM_PAUSE PuffyEvtStreamPause;
EVT_ACX_STREAM_ALLOCATE_RTPACKETS PuffyEvtStreamAllocateRtPackets;
EVT_ACX_STREAM_FREE_RTPACKETS PuffyEvtStreamFreeRtPackets;
EVT_ACX_STREAM_GET_HW_LATENCY PuffyEvtStreamGetHwLatency;
EVT_ACX_STREAM_GET_CURRENT_PACKET PuffyEvtStreamGetCurrentPacket;
EVT_ACX_STREAM_GET_PRESENTATION_POSITION PuffyEvtStreamGetPresentationPosition;
EVT_ACX_STREAM_SET_RENDER_PACKET PuffyEvtStreamSetRenderPacket;
EVT_ACX_STREAM_GET_CAPTURE_PACKET PuffyEvtStreamGetCapturePacket;
EVT_WDF_TIMER PuffyEvtPacketTimer;
EVT_WDF_OBJECT_CONTEXT_CLEANUP PuffyEvtStreamCleanup;

} // namespace puffy::virtual_audio::driver
