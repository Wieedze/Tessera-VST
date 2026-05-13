<#
.SYNOPSIS
    Copies the freshly built Tessera VST3 bundle to the Ableton-scanned folder.

.DESCRIPTION
    After 'cmake --build Builds-Win --config Release', the VST3 sits in
    Builds-Win\Tessera_artefacts\Release\VST3\Tessera.vst3 but is not yet
    installed anywhere the host can find it.

    This script copies the bundle to a configurable destination. By default
    it uses %APPDATA%\VST3 which is writable by the current user (no admin
    required) and can be picked up by Ableton via:
        Preferences -> Plug-Ins -> Use VST3 Plug-In Custom Folder -> Browse

    If the host has the DLL locked (plugin currently loaded), the script
    detects the failure and prints a clear instruction instead of crashing
    with an ugly Permission Denied.

.PARAMETER Destination
    Override the install destination. Defaults to "$env:APPDATA\VST3".

.PARAMETER SourceBuildDir
    Override the build directory. Defaults to "Builds-Win" relative to the
    script's location.

.EXAMPLE
    .\scripts\install-vst3.ps1
    .\scripts\install-vst3.ps1 -Destination "C:\Users\Max\Documents\VST3"

.NOTES
    Maxime / Tessera-VST. Part of sprint W3 phase 1 workflow polish.
#>

param(
    [string]$Destination    = "$env:APPDATA\VST3",
    [string]$SourceBuildDir = ""
)

$ErrorActionPreference = "Stop"

# Resolve script location -> repo root (the script lives at <repo>/scripts/)
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = Split-Path -Parent $scriptDir
if ([string]::IsNullOrEmpty($SourceBuildDir)) {
    $SourceBuildDir = Join-Path $repoRoot "Builds-Win"
}

$sourceBundle = Join-Path $SourceBuildDir "Tessera_artefacts\Release\VST3\Tessera.vst3"

# Sanity checks
if (-not (Test-Path $sourceBundle)) {
    Write-Host "ERROR: Source bundle not found at:" -ForegroundColor Red
    Write-Host "  $sourceBundle"
    Write-Host ""
    Write-Host "Build the plugin first:"
    Write-Host "  cmake --build Builds-Win --config Release" -ForegroundColor Yellow
    exit 1
}

# Make sure destination folder exists
if (-not (Test-Path $Destination)) {
    Write-Host "Creating destination folder: $Destination"
    New-Item -ItemType Directory -Path $Destination | Out-Null
}

# The bundle is a folder with extension .vst3; the actual locked DLL is
# inside Contents\x86_64-win\Tessera.vst3. Detect lock by trying to open
# that file for exclusive write — much cleaner than waiting for Copy-Item
# to fail mid-copy.
$lockProbe = Join-Path $Destination "Tessera.vst3\Contents\x86_64-win\Tessera.vst3"
if (Test-Path $lockProbe) {
    try {
        $fs = [System.IO.File]::Open($lockProbe, 'Open', 'Write', 'None')
        $fs.Close()
    }
    catch [System.IO.IOException] {
        Write-Host "ERROR: $lockProbe is locked by another process." -ForegroundColor Red
        Write-Host ""
        Write-Host "This typically means a host (Ableton, Reaper, Studio One...) has" -ForegroundColor Yellow
        Write-Host "Tessera loaded. To fix:" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "  Option 1 - Close the host completely (quit, not just the project)."
        Write-Host "  Option 2 - Remove the Tessera device from every track + tracks's freeze."
        Write-Host "  Option 3 - In Ableton: Ctrl+J on the device toggles it inactive,"
        Write-Host "             which usually releases the DLL handle."
        Write-Host ""
        Write-Host "Then rerun: .\scripts\install-vst3.ps1"
        exit 2
    }
}

# Copy the bundle. -Recurse copies the full Contents tree.
Write-Host "Installing Tessera.vst3 -> $Destination" -ForegroundColor Cyan
Copy-Item -Recurse -Force $sourceBundle $Destination

# Verify the binary landed where it should
$installedBinary = Join-Path $Destination "Tessera.vst3\Contents\x86_64-win\Tessera.vst3"
if (Test-Path $installedBinary) {
    $size = (Get-Item $installedBinary).Length
    Write-Host "OK - Tessera.vst3 installed (binary size: $size bytes)" -ForegroundColor Green
    Write-Host ""
    Write-Host "Next step: in Ableton, click 'Rescan Plug-Ins' in Preferences > Plug-Ins."
    Write-Host "If the device was already loaded, remove it and drag a new instance."
    exit 0
} else {
    Write-Host "ERROR: Copy completed but the binary is missing at:" -ForegroundColor Red
    Write-Host "  $installedBinary"
    exit 3
}
