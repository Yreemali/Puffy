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
    throw 'Driver removal requires an elevated PowerShell window (Run as administrator).'
}

if (-not $DriverDirectory) {
    $DriverDirectory = Join-Path $PSScriptRoot "build\$Platform\$Configuration"
}
$DriverDirectory = (Resolve-Path $DriverDirectory).Path
$deviceTool = Join-Path $DriverDirectory 'PuffyVirtualAudioDevice.exe'
if (-not (Test-Path $deviceTool)) {
    throw "Root-device helper is missing: $deviceTool"
}

Write-Host '==> Removing ROOT\PuffyVirtualAudio'
& $deviceTool remove
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0 -and $exitCode -ne 10) {
    throw "PuffyVirtualAudioDevice.exe failed with exit code $exitCode."
}

Write-Host 'Puffy Virtual Audio device removed.' -ForegroundColor Green
Write-Host 'The signed package may remain in Driver Store, which is normal and allows Windows to service/update the driver.'
if ($exitCode -eq 10) {
    Write-Warning 'Windows reported that a reboot is required to finish removing the device.'
}
