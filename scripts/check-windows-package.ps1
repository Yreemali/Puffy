[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ReleaseDir
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$ReleaseDir = (Resolve-Path $ReleaseDir).Path

$required = @(
    (Join-Path $ReleaseDir 'Puffy.exe'),
    (Join-Path $ReleaseDir 'puffy_native.dll')
)
foreach ($path in $required) {
    if (-not (Test-Path $path -PathType Leaf)) {
        throw "Missing required Windows runtime file: $path"
    }
}

$installerDir = Join-Path $ReleaseDir 'bundle\nsis'
$installer = Get-ChildItem $installerDir -Filter '*-setup.exe' -File -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $installer) {
    throw "No NSIS setup executable found under $installerDir"
}

$dll = Get-Item (Join-Path $ReleaseDir 'puffy_native.dll')
if ($dll.Length -lt 100KB) {
    throw "puffy_native.dll is unexpectedly small ($($dll.Length) bytes); native packaging is likely broken."
}

Write-Host "Validated Windows runtime: $ReleaseDir"
Write-Host "Validated installer: $($installer.FullName)"
