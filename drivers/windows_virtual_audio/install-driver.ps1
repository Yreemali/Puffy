[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('x64')]
    [string]$Platform = 'x64',
    [string]$DriverDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Driver installation requires an elevated PowerShell window (Run as administrator).'
}

if (-not $DriverDirectory) {
    $DriverDirectory = Join-Path $PSScriptRoot "build\$Platform\$Configuration"
}
$DriverDirectory = (Resolve-Path $DriverDirectory).Path

$inf = Join-Path $DriverDirectory 'PuffyVirtualAudio.inf'
$sys = Join-Path $DriverDirectory 'PuffyVirtualAudio.sys'
$cat = Join-Path $DriverDirectory 'PuffyVirtualAudio.cat'
foreach ($file in @($inf, $sys, $cat)) {
    if (-not (Test-Path $file)) { throw "Driver package is incomplete: $file" }
}

$signature = Get-AuthenticodeSignature $cat
if ($signature.Status -ne [System.Management.Automation.SignatureStatus]::Valid) {
    throw "PuffyVirtualAudio.cat does not have a valid trusted Authenticode signature (status: $($signature.Status)). Sign/trust the driver package before installing it."
}

$deviceTool = Join-Path $DriverDirectory 'PuffyVirtualAudioDevice.exe'
if (-not (Test-Path $deviceTool)) {
    throw "Root-device installer helper is missing: $deviceTool"
}

Write-Host '==> Creating/updating ROOT\PuffyVirtualAudio and installing its signed driver'
& $deviceTool install $inf
$deviceToolExit = $LASTEXITCODE
if ($deviceToolExit -ne 0 -and $deviceToolExit -ne 10) {
    throw "PuffyVirtualAudioDevice.exe failed with exit code $deviceToolExit."
}

Write-Host ''
Write-Host 'Driver installation completed.' -ForegroundColor Green
Write-Host 'Expected endpoints:'
Write-Host '  Render : Puffy Virtual Microphone Transport'
Write-Host '  Capture: Puffy Virtual Microphone'
if ($deviceToolExit -eq 10) {
    Write-Warning 'Windows reported that a reboot is required before the device is fully ready.'
}
else {
    Write-Host 'Restart Puffy and any audio client that was already open.'
}
