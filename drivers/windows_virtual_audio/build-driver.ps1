[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('x64')]
    [string]$Platform = 'x64',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$DriverRoot = (Resolve-Path $PSScriptRoot).Path
$Solution = Join-Path $DriverRoot 'PuffyVirtualAudio.sln'
$Output = Join-Path $DriverRoot "build\$Platform\$Configuration"

function Find-MSBuild {
    $direct = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($direct) { return $direct.Source }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $path = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' |
            Select-Object -First 1
        if ($path -and (Test-Path $path)) { return $path }
    }

    throw 'MSBuild was not found. Install Visual Studio 2022 with Desktop development with C++ and the Windows Driver Kit (WDK).'
}

function Find-Inf2Cat {
    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    if (-not (Test-Path $kitsRoot)) { return $null }

    $candidate = Get-ChildItem $kitsRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^10\.0\.\d+\.\d+$' } |
        Sort-Object { [version]$_.Name } -Descending |
        ForEach-Object { Join-Path $_.FullName 'x64\Inf2Cat.exe' } |
        Where-Object { Test-Path $_ } |
        Select-Object -First 1

    return $candidate
}

if (-not (Test-Path $Solution)) { throw "Driver solution is missing: $Solution" }

if ($Clean) {
    Remove-Item (Join-Path $DriverRoot 'build') -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $DriverRoot 'obj') -Recurse -Force -ErrorAction SilentlyContinue
}

$msbuild = Find-MSBuild
Write-Host "==> Building Puffy Virtual Audio ($Configuration|$Platform)"
& $msbuild $Solution /m /t:Build "/p:Configuration=$Configuration" "/p:Platform=$Platform" /nologo
if ($LASTEXITCODE -ne 0) { throw 'WDK/MSBuild driver build failed.' }

New-Item -ItemType Directory -Path $Output -Force | Out-Null

$sourceInf = Join-Path $DriverRoot 'PuffyVirtualAudio.inf'
$outInf = Join-Path $Output 'PuffyVirtualAudio.inf'
$outSys = Join-Path $Output 'PuffyVirtualAudio.sys'
$outCat = Join-Path $Output 'PuffyVirtualAudio.cat'
$outDeviceTool = Join-Path $Output 'PuffyVirtualAudioDevice.exe'

# Always stage the current INF. Reusing a previous output here can create a
# catalog for an older package after an incremental rebuild.
Copy-Item $sourceInf $outInf -Force
if (-not (Test-Path $outSys)) {
    throw "Driver binary was not produced: $outSys"
}
if (-not (Test-Path $outDeviceTool)) {
    throw "Root-device installer helper was not produced: $outDeviceTool"
}

$inf2cat = Find-Inf2Cat
if (-not $inf2cat) {
    throw 'Inf2Cat.exe was not found. Install a current Windows Driver Kit.'
}

# A CAT contains hashes of the package files. It must be regenerated after
# every SYS/INF rebuild; keeping a stale CAT produces a package whose signature
# can look valid while no longer covering the current driver binary.
Remove-Item $outCat -Force -ErrorAction SilentlyContinue
Write-Host '==> Validating INF and generating a fresh driver catalog'
$inf2CatOs = '10_VB_X64,10_CO_X64,10_NI_X64,10_GE_X64,10_25H2_X64'
& $inf2cat "/driver:$Output" "/os:$inf2CatOs" /uselocaltime /verbose
if ($LASTEXITCODE -ne 0) { throw 'Inf2Cat failed to validate the INF or generate PuffyVirtualAudio.cat.' }

foreach ($file in @($outSys, $outInf, $outCat, $outDeviceTool)) {
    if (-not (Test-Path $file)) { throw "Driver package is incomplete: $file" }
}

Write-Host ''
Write-Host 'Puffy Virtual Audio driver package built.' -ForegroundColor Green
Write-Host "SYS: $outSys"
Write-Host "INF: $outInf"
Write-Host "CAT: $outCat"
Write-Host "Tool: $outDeviceTool"
Write-Host ''
Write-Warning 'A generated CAT is not automatically production-signed. Sign the driver package through the Microsoft driver-signing process before distributing it to normal Windows users.'
