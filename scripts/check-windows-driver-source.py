#!/usr/bin/env python3
"""Cross-platform consistency checks for the Puffy Windows virtual-audio source."""
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
DRIVER = ROOT / "drivers" / "windows_virtual_audio"

contract = (ROOT / "virtual_audio" / "virtual_device_contract.hpp").read_text(encoding="utf-8")
header = (DRIVER / "PuffyVirtualAudio.hpp").read_text(encoding="utf-8")
source = (DRIVER / "PuffyVirtualAudio.cpp").read_text(encoding="utf-8")
inf = (DRIVER / "PuffyVirtualAudio.inf").read_text(encoding="utf-8")
project_path = DRIVER / "PuffyVirtualAudio.vcxproj"
device_project_path = DRIVER / "PuffyVirtualAudioDevice.vcxproj"
device_tool_source = (DRIVER / "PuffyVirtualAudioDevice.cpp").read_text(encoding="utf-8")
build_driver = (DRIVER / "build-driver.ps1").read_text(encoding="utf-8")
windows_build = (ROOT / "scripts" / "build-windows.ps1").read_text(encoding="utf-8")
uninstall_driver = (DRIVER / "uninstall-driver.ps1").read_text(encoding="utf-8")
tauri_script = (ROOT / "ui" / "web" / "scripts" / "tauri.mjs").read_text(encoding="utf-8")
nsis_hooks = (ROOT / "ui" / "web" / "src-tauri" / "windows" / "driver-hooks.nsh").read_text(encoding="utf-8")
windows_backend = (ROOT / "platform" / "windows" / "windows_audio_backend.cpp").read_text(encoding="utf-8")
native_bridge = (ROOT / "native" / "puffy_native.cpp").read_text(encoding="utf-8")


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def contract_string(name: str) -> str:
    match = re.search(rf'{name}\s*=\s*"([^"]+)"', contract)
    expect(match is not None, f"Missing {name} in virtual device contract")
    return match.group(1)


def contract_int(name: str) -> int:
    match = re.search(rf'{name}\s*=\s*([0-9\'_,]+)', contract)
    expect(match is not None, f"Missing {name} in virtual device contract")
    return int(match.group(1).replace("'", "").replace("_", "").replace(",", ""))


public_name = contract_string("publicMicrophoneName")
transport_name = contract_string("windowsTransportFriendlyName")
sample_rate = contract_int("transportSampleRate")
channels = contract_int("transportChannels")

expect(f'MicrophoneFriendlyName="{public_name}"' in inf, "INF public microphone name does not match the app contract")
expect(f'TransportFriendlyName="{transport_name}"' in inf, "INF transport name does not match the app contract")
expect("NTamd64.10.0...19041" in inf, "INF must target Windows 10 build 19041 or newer for ACX 1.1")
expect("KmdfLibraryVersion=1.31" in inf, "INF must request KMDF 1.31 for ACX 1.1")
expect("{1DA5D803-D492-4EDD-8C23-E0C0FFEE7F0E},7" in inf, "INF event-driven endpoint property key is incorrect")
expect(f"kSampleRate = {sample_rate // 1000}'{sample_rate % 1000:03d}" in header or f"kSampleRate = {sample_rate}" in header,
       "Driver sample rate does not match the app contract")
expect(f"kChannels = {channels}" in header, "Driver channel count does not match the app contract")
expect("AcxCircuitTypeRender" in source and "AcxCircuitTypeCapture" in source, "Both render and capture circuits are required")
expect("} else {\n    } else {" not in source, "Driver source contains a duplicated else branch")
expect("AcxRtStreamNotifyPacketComplete" in source, "RT packet completion notification is missing")
expect("ringCopyIn" in source and "ringCopyOut" in source, "Virtual cable ring transport is missing")
expect("kTimerQuantumMilliseconds = 2" in source, "Timer-driven WaveRT transport must advance in small sub-packet chunks")
expect("writeTransportChunk" in source and "renderPacketReady" in source and "renderReadyPacket" in header,
       "Render WaveRT readiness tracking/silence fallback is missing")
expect("publishPresentation" in source and "presentationSequence" in header,
       "Coherent presentation position/QPC publication is missing")
expect("STATUS_INVALID_DEVICE_STATE" in source and "eosReceived" in source,
       "Render EOS state handling is incomplete")
expect("RtlSizeTMult" in source and "RtlULongAdd" in source,
       "RT packet allocation needs checked integer arithmetic")
expect("distance == 0 && currentOffset != 0" in source,
       "SetRenderPacket must reject a packet once that packet is already transferring")
expect("*lastCapturePacket = current - 1" in source,
       "Capture packet reporting must preserve the pre-first-packet sentinel")

ET.parse(project_path)
ET.parse(device_project_path)
project = project_path.read_text(encoding="utf-8")
device_project = device_project_path.read_text(encoding="utf-8")
expect("WindowsKernelModeDriver10.0" in project, "WDK kernel-mode toolset is not configured")
expect("<ACX_VERSION_MINOR>1</ACX_VERSION_MINOR>" in project, "ACX 1.1 is not configured")
expect("PuffyVirtualAudio.cpp" in project and "PuffyVirtualAudio.inf" in project, "Driver project does not package required sources")
expect("PuffyVirtualAudioDevice.cpp" in device_project, "Root-device installer project is missing its source")
expect("#define UNICODE" not in device_tool_source and "#define _UNICODE" not in device_tool_source,
       "Device helper must not redefine Unicode macros already supplied by MSBuild (/WX build)")
expect("SetupDiCreateDeviceInfoW" in device_tool_source and "DIF_REGISTERDEVICE" in device_tool_source,
       "Installer helper must create/register the root devnode")
expect("UpdateDriverForPlugAndPlayDevicesW" in device_tool_source,
       "Installer helper must bind the INF to ROOT\\PuffyVirtualAudio")


expect("Remove-Item $outCat" in build_driver, "Driver build must discard stale catalogs before Inf2Cat")
expect("10_VB_X64,10_CO_X64,10_NI_X64,10_GE_X64,10_25H2_X64" in build_driver,
       "Inf2Cat OS coverage is incomplete for supported x64 Windows 10/11 releases")
expect("/uselocaltime" in build_driver, "Inf2Cat should use local time to avoid DriverVer timezone false positives")
expect("Find-SignTool" in windows_build and "verify /kp /v /c" in windows_build,
       "Release packaging must verify SYS/INF against the exact signed catalog")
expect("PuffyVirtualAudioDevice.exe" in uninstall_driver and " remove" in uninstall_driver,
       "Standalone driver removal helper is missing")
expect("PUFFY_SIGNED_DRIVER_DIR" in tauri_script and "installerHooks" in tauri_script and "perMachine" in tauri_script,
       "Signed driver package is not wired into the elevated NSIS release build")
expect("NSIS_HOOK_POSTINSTALL" in nsis_hooks and "NSIS_HOOK_PREUNINSTALL" in nsis_hooks,
       "NSIS driver install/uninstall hooks are incomplete")
expect("virtual_audio::windowsTransportFriendlyName" in windows_backend and
       "output_->open(endpointId_, format_)" in windows_backend,
       "Windows app is not resolving/opening the driver transport endpoint")
expect("WindowsVirtualMicrophone microphone" in native_bridge and
       "context->engine.start(context->capture, context->output, &context->microphone)" in native_bridge,
       "Native Windows graph is not wiring the virtual microphone into AudioEngine")

print("Windows virtual-audio source checks passed.")
