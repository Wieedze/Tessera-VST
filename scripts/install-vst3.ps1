<#
.SYNOPSIS
    Installs the freshly built Tessera VST3 bundle into every Ableton-scanned path.

.DESCRIPTION
    After 'cmake --build Builds-Win --config Release', the VST3 sits in
    Builds-Win\Tessera_artefacts\Release\VST3\Tessera.vst3 but is not yet
    installed anywhere the host can find it.

    This script copies the bundle to each of the well-known VST3 destinations
    so Ableton (or any other VST3 host) loads the latest version no matter
    which folder its scanner reaches first:

      1. %APPDATA%\VST3                       (user, always writable)
      2. C:\Program Files\Common Files\VST3   (system default, admin needed)
      3. C:\Program Files (x86)\VSTPlugIns    (Maxime's legacy custom folder)

    Each destination is independent. If admin is missing, the admin-only
    paths are skipped with a clear note. If a host holds the DLL locked,
    the script prints an instruction and moves on. The script never crashes.

.PARAMETER Destination
    Override the PRIMARY install destination. Defaults to "$env:APPDATA\VST3".

.PARAMETER ExtraDestinations
    Override the list of additional destinations. Defaults to the two
    admin-only paths above.

.PARAMETER SourceBuildDir
    Override the build directory. Defaults to "Builds-Win" relative to
    the script's location.

.EXAMPLE
    .\scripts\install-vst3.ps1
    .\scripts\install-vst3.ps1 -Destination "D:\MyPlugins\VST3"
    .\scripts\install-vst3.ps1 -ExtraDestinations @()       # only the user folder

.NOTES
    Tessera-VST. Updated S3 (W3) to handle multi-destination install +
    admin-aware copy. See .claude/lessons/0013-ableton-live-database-cache.md
    for why we want to install in every scanned path.
#>

param(
    [string]   $Destination        = "$env:APPDATA\VST3",
    [string[]] $ExtraDestinations  = @(
        "C:\Program Files\Common Files\VST3",
        "C:\Program Files (x86)\VSTPlugIns"
    ),
    [string]   $SourceBuildDir     = ""
)

$ErrorActionPreference = "Stop"

# ---- 1. Resolve source path --------------------------------------------------

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = Split-Path -Parent $scriptDir
if ([string]::IsNullOrEmpty($SourceBuildDir)) {
    $SourceBuildDir = Join-Path $repoRoot "Builds-Win"
}

$sourceBundle = Join-Path $SourceBuildDir "Tessera_artefacts\Release\VST3\Tessera.vst3"

if (-not (Test-Path $sourceBundle)) {
    Write-Host "ERROR: Source bundle not found at:" -ForegroundColor Red
    Write-Host "  $sourceBundle"
    Write-Host ""
    Write-Host "Build the plugin first:"
    Write-Host "  cmake --build Builds-Win --config Release" -ForegroundColor Yellow
    exit 1
}

$sourceBinary = Join-Path $sourceBundle "Contents\x86_64-win\Tessera.vst3"
$sourceBinarySize = (Get-Item $sourceBinary -ErrorAction SilentlyContinue).Length
Write-Host "Source: $sourceBundle ($sourceBinarySize bytes)" -ForegroundColor Cyan
Write-Host ""

# ---- 2. Helpers --------------------------------------------------------------

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]"Administrator")

function Test-RequiresAdmin {
    param([string]$Path)
    # Heuristic: anything under C:\Program Files or C:\Program Files (x86) needs admin.
    return ($Path -like "C:\Program Files*")
}

function Test-DllLocked {
    param([string]$BundleParent)
    $probe = Join-Path $BundleParent "Tessera.vst3\Contents\x86_64-win\Tessera.vst3"
    if (-not (Test-Path $probe)) { return $false }
    try {
        $fs = [System.IO.File]::Open($probe, 'Open', 'Write', 'None')
        $fs.Close()
        return $false
    } catch [System.IO.IOException] {
        return $true
    }
}

function Install-ToPath {
    param([string]$DestParent)

    Write-Host "→ Installing to $DestParent" -ForegroundColor Cyan

    if ((Test-RequiresAdmin $DestParent) -and (-not $isAdmin)) {
        Write-Host "  SKIPPED — requires admin (run PowerShell as administrator to enable this destination)." -ForegroundColor Yellow
        return $false
    }

    if (Test-DllLocked $DestParent) {
        Write-Host "  SKIPPED — DLL is locked by a host. Close Ableton / disable the device and rerun." -ForegroundColor Yellow
        return $false
    }

    try {
        if (-not (Test-Path $DestParent)) {
            New-Item -ItemType Directory -Path $DestParent | Out-Null
        }
        Copy-Item -Recurse -Force $sourceBundle $DestParent
        $installed = Join-Path $DestParent "Tessera.vst3\Contents\x86_64-win\Tessera.vst3"
        if (Test-Path $installed) {
            Write-Host "  OK — installed ($((Get-Item $installed).Length) bytes)." -ForegroundColor Green
            return $true
        } else {
            Write-Host "  ERROR — copy completed but binary missing at $installed" -ForegroundColor Red
            return $false
        }
    } catch {
        Write-Host "  ERROR — $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

# ---- 3. Install ---------------------------------------------------------------

if (-not $isAdmin) {
    Write-Host "(running without admin — paths under Program Files will be skipped)" -ForegroundColor DarkGray
    Write-Host ""
}

$successCount = 0
$attemptCount = 0

# Primary destination (the documented default)
$attemptCount++
if (Install-ToPath -DestParent $Destination) { $successCount++ }

# Extra destinations (Ableton's other scanned paths)
foreach ($extra in $ExtraDestinations) {
    $attemptCount++
    if (Install-ToPath -DestParent $extra) { $successCount++ }
}

# ---- 4. Summary --------------------------------------------------------------

Write-Host ""
Write-Host "Installed to $successCount / $attemptCount destinations." -ForegroundColor Cyan

if ($successCount -eq 0) {
    Write-Host ""
    Write-Host "No destination accepted the install. Options:" -ForegroundColor Red
    Write-Host "  - Close Ableton (or disable the Tessera device) and retry."
    Write-Host "  - Run PowerShell as administrator to enable Program Files paths."
    exit 2
}

Write-Host ""
Write-Host "Next step in Ableton:"
Write-Host "  Preferences > Plug-Ins > 'Rescan Plug-Ins'"
Write-Host "  Then remove + re-drag the Tessera device on your track to load the new version."
exit 0
