[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string]$Architecture = 'x64',
    [switch]$SkipTests,
    [switch]$SkipDriver,
    [string]$SignedDriverDirectory,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$WebRoot = Join-Path $ProjectRoot 'ui\web'
$NativeBuild = Join-Path $ProjectRoot 'build-windows-release'
$Triplet = 'x64-windows-static'
$DriverBuildScript = Join-Path $ProjectRoot 'drivers\windows_virtual_audio\build-driver.ps1'
$DriverOutput = Join-Path $ProjectRoot 'drivers\windows_virtual_audio\build\x64\Release'



function Find-SignTool {
    $direct = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($direct) { return $direct.Source }

    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    if (-not (Test-Path $kitsRoot)) { return $null }
    return Get-ChildItem $kitsRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^10\.0\.\d+\.\d+$' } |
        Sort-Object { [version]$_.Name } -Descending |
        ForEach-Object { Join-Path $_.FullName 'x64\signtool.exe' } |
        Where-Object { Test-Path $_ } |
        Select-Object -First 1
}

function Resolve-SignedDriverPackage([string]$Directory) {
    if (-not $Directory) { return $null }
    $resolved = (Resolve-Path $Directory).Path
    $required = @(
        'PuffyVirtualAudio.sys',
        'PuffyVirtualAudio.inf',
        'PuffyVirtualAudio.cat',
        'PuffyVirtualAudioDevice.exe'
    )
    foreach ($name in $required) {
        $path = Join-Path $resolved $name
        if (-not (Test-Path $path)) { throw "Signed driver package is incomplete: $path" }
    }

    $catalog = Join-Path $resolved 'PuffyVirtualAudio.cat'
    $signature = Get-AuthenticodeSignature $catalog
    if ($signature.Status -ne [System.Management.Automation.SignatureStatus]::Valid) {
        throw "The driver CAT is not valid/trusted (status: $($signature.Status)): $catalog"
    }

    # A valid CAT signature alone is not enough: the SYS/INF could have changed
    # after the catalog was signed. Verify each package member against this
    # exact catalog under Windows kernel signing policy before it is bundled.
    $signtool = Find-SignTool
    if (-not $signtool) {
        throw 'SignTool.exe was not found. Install a current Windows SDK/WDK to verify the signed driver package.'
    }
    foreach ($member in @('PuffyVirtualAudio.sys', 'PuffyVirtualAudio.inf')) {
        $memberPath = Join-Path $resolved $member
        & $signtool verify /kp /v /c $catalog $memberPath
        if ($LASTEXITCODE -ne 0) {
            throw "The signed driver catalog does not kernel-policy-validate this package member: $memberPath"
        }
    }
    return $resolved
}

function Require-Command([string]$Name, [string]$Hint) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required command '$Name' was not found. $Hint"
    }
}

Require-Command 'cmake' 'Install CMake and add it to PATH.'
Require-Command 'node' 'Install Node.js 22 LTS or newer.'
Require-Command 'npm' 'Install Node.js with npm.'
Require-Command 'cargo' 'Install the stable Rust toolchain with rustup.'
Require-Command 'rustc' 'Install the stable Rust toolchain with rustup.'
Require-Command 'vcpkg' 'Install vcpkg and add vcpkg.exe to PATH.'

$vcpkgCommand = (Get-Command vcpkg).Source
if (-not $env:VCPKG_ROOT) {
    $candidate = Split-Path $vcpkgCommand -Parent
    if (Test-Path (Join-Path $candidate 'scripts\buildsystems\vcpkg.cmake')) {
        $env:VCPKG_ROOT = $candidate
    }
}
if (-not $env:VCPKG_ROOT -or -not (Test-Path (Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'))) {
    throw 'VCPKG_ROOT is not set to a valid vcpkg checkout.'
}
$env:VCPKG_TARGET_TRIPLET = $Triplet

$ResolvedSignedDriverDirectory = Resolve-SignedDriverPackage $SignedDriverDirectory
if ($ResolvedSignedDriverDirectory) {
    $env:PUFFY_SIGNED_DRIVER_DIR = $ResolvedSignedDriverDirectory
}
else {
    Remove-Item Env:PUFFY_SIGNED_DRIVER_DIR -ErrorAction SilentlyContinue
}

if ($Clean) {
    Remove-Item $NativeBuild -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $ProjectRoot 'build-tauri-native') -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $WebRoot 'src-tauri\target') -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $WebRoot 'src-tauri\native') -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host '==> Installing native Windows dependencies'
& vcpkg install "sqlite3:$Triplet" "libsndfile:$Triplet"
if ($LASTEXITCODE -ne 0) { throw 'vcpkg dependency installation failed.' }

if (-not $SkipDriver) {
    if (-not (Test-Path $DriverBuildScript)) { throw "Windows virtual-audio driver build helper is missing: $DriverBuildScript" }
    Write-Host '==> Building Windows virtual microphone driver'
    if ($Clean) {
        & $DriverBuildScript -Configuration Release -Platform x64 -Clean
    }
    else {
        & $DriverBuildScript -Configuration Release -Platform x64
    }
    if (-not (Test-Path (Join-Path $DriverOutput 'PuffyVirtualAudio.sys'))) {
        throw 'PuffyVirtualAudio.sys was not produced.'
    }
    if (-not (Test-Path (Join-Path $DriverOutput 'PuffyVirtualAudio.inf'))) {
        throw 'PuffyVirtualAudio.inf was not produced.'
    }
    if (-not (Test-Path (Join-Path $DriverOutput 'PuffyVirtualAudio.cat'))) {
        throw 'PuffyVirtualAudio.cat was not produced.'
    }
    if (-not (Test-Path (Join-Path $DriverOutput 'PuffyVirtualAudioDevice.exe'))) {
        throw 'PuffyVirtualAudioDevice.exe was not produced.'
    }
}

if (-not $SkipTests) {
    Write-Host '==> Configuring C++ core'
    & cmake -S $ProjectRoot -B $NativeBuild `
        '-DPUFFY_BUILD_TESTS=ON' `
        "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
        "-DVCPKG_TARGET_TRIPLET=$Triplet"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }

    Write-Host '==> Building C++ core and tests'
    & cmake --build $NativeBuild --config Release --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }

    Write-Host '==> Running C++ tests'
    & ctest --test-dir $NativeBuild -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Native tests failed.' }
}

Write-Host '==> Installing web dependencies'
Push-Location $WebRoot
try {
    & npm ci
    if ($LASTEXITCODE -ne 0) { throw 'npm ci failed.' }

    Write-Host '==> Building Puffy NSIS installer'
    & npm run tauri:build -- --bundles nsis
    if ($LASTEXITCODE -ne 0) { throw 'Tauri/NSIS build failed.' }
}
finally {
    Pop-Location
}

$ReleaseDir = Join-Path $WebRoot 'src-tauri\target\release'
$NativeDll = Join-Path $ReleaseDir 'puffy_native.dll'
$AppExe = Join-Path $ReleaseDir 'Puffy.exe'
$InstallerDir = Join-Path $ReleaseDir 'bundle\nsis'
$Installer = Get-ChildItem $InstallerDir -Filter '*-setup.exe' -File -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-not (Test-Path $AppExe)) { throw "Built application executable is missing: $AppExe" }
if (-not (Test-Path $NativeDll)) { throw "Native runtime DLL is missing beside Puffy.exe: $NativeDll" }
if (-not $Installer) { throw "NSIS installer was not produced under: $InstallerDir" }

Write-Host ''
Write-Host 'Windows build completed successfully.' -ForegroundColor Green
Write-Host "Application: $AppExe"
Write-Host "Native DLL : $NativeDll"
Write-Host "Installer  : $($Installer.FullName)"
if (-not $SkipDriver) {
    Write-Host "Driver SYS : $(Join-Path $DriverOutput 'PuffyVirtualAudio.sys')"
    Write-Host "Driver INF : $(Join-Path $DriverOutput 'PuffyVirtualAudio.inf')"
    Write-Host "Driver CAT : $(Join-Path $DriverOutput 'PuffyVirtualAudio.cat')"
    Write-Host "Device tool: $(Join-Path $DriverOutput 'PuffyVirtualAudioDevice.exe')"
}
Write-Host ''
if ($ResolvedSignedDriverDirectory) {
    Write-Host "Bundled signed driver: $ResolvedSignedDriverDirectory" -ForegroundColor Green
    Write-Host 'The NSIS installer is configured per-machine and installs/removes the virtual-audio device through its signed driver helper.'
}
elseif ($SkipDriver) {
    Write-Warning 'Application build completed without a bundled Windows virtual microphone driver (-SkipDriver and no -SignedDriverDirectory).'
}
else {
    Write-Warning 'The locally built driver package is unsigned and is therefore NOT bundled into NSIS. For a release installer, sign the SYS/INF/CAT package through the Microsoft driver-signing process and rebuild with -SignedDriverDirectory <signed-package-path>.'
}
