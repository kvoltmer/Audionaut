# Audionaut - Audio editing application for multitrack recordings.
# Copyright (C) 2025 Klaus Voltmer
#
# Audionaut uses a GPL/commercial licence - see LICENCE.md for details.
#
# Windows counterpart of build_essentia.sh. Builds Essentia as a static MSVC
# library so the analysis features (BIC segmentation, onset detection, beat
# tracking) are available on Windows.
#
# build_essentia.sh cannot be used here: it drives Essentia's waf build, whose
# win32 branch is dead code (the whole body sits inside a triple-quoted string
# in wscript), and its 3rd-party scripts are bash targeting MinGW, which emits
# archives MSVC cannot link. This script instead builds Essentia's sources with
# CMake (Builds/Windows/CMakeLists.txt) against dependencies from vcpkg.
#
# The Essentia submodule is never modified - unlike the macOS path, which has to
# patch its working tree.
#
# Usage:
#   ./Audionaut/Builds/build_essentia_windows.ps1
#   ./Audionaut/Builds/build_essentia_windows.ps1 -Configuration Debug
#
# Prerequisites: Visual Studio with the C++ toolset, CMake, and Python 3 (any
# version - only the standard library is used; waf's distutils requirement does
# not apply). vcpkg is cloned and bootstrapped automatically if -VcpkgRoot is
# absent.

[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',

    [string]$VcpkgRoot = "$PSScriptRoot\..\..\..\vcpkg",

    [string]$Triplet = 'x64-windows-static-md'
)

$ErrorActionPreference = 'Stop'

$repoRoot     = (Resolve-Path "$PSScriptRoot\..\..").Path
$essentiaRoot = Join-Path $repoRoot 'Submodules\essentia'
$buildDir     = Join-Path $essentiaRoot 'build-msvc'
$cmakeSource  = Join-Path $PSScriptRoot 'Windows'

if (-not (Test-Path (Join-Path $essentiaRoot 'src'))) {
    throw "Essentia submodule not checked out at $essentiaRoot. Run: git submodule update --init --recursive"
}

# CMake is not necessarily on PATH - a stock Visual Studio install ships it but
# only exposes it inside a developer prompt. Fall back to the bundled copy so
# the script works from an ordinary shell.
$cmakeExe = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmakeExe) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $cmakeExe = & $vswhere -latest -prerelease -products * `
            -find 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' |
            Select-Object -First 1
    }
}
if (-not $cmakeExe) {
    throw "CMake not found on PATH or in the Visual Studio install. Install CMake or run from a Developer PowerShell."
}
Write-Host "-> cmake at $cmakeExe"

# vcpkg -----------------------------------------------------------------------
if (-not (Test-Path (Join-Path $VcpkgRoot 'vcpkg.exe'))) {
    Write-Host "-> vcpkg not found at $VcpkgRoot; cloning and bootstrapping"
    if (-not (Test-Path $VcpkgRoot)) {
        git clone --depth 1 https://github.com/microsoft/vcpkg.git $VcpkgRoot
    }
    & (Join-Path $VcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
    if ($LASTEXITCODE -ne 0) { throw "vcpkg bootstrap failed" }
}
$VcpkgRoot = (Resolve-Path $VcpkgRoot).Path
Write-Host "-> vcpkg at $VcpkgRoot"

# The dependency set mirrors build_essentia.sh's
# --lightweight=fftw,libav,libsamplerate,yaml. eigen3 replaces the copy that
# Essentia's 3rd-party scripts would otherwise vendor.
#
# The triplet matters: x64-windows-static-md gives static libraries against the
# dynamic CRT (/MD), which is what both Audionaut.vcxproj and the JUCE CMake
# test target use. A mismatch here surfaces as duplicate-symbol or
# _ITERATOR_DEBUG_LEVEL link errors much later.
Write-Host "-> Installing dependencies ($Triplet) - the ffmpeg build is slow on a cold cache"
& (Join-Path $VcpkgRoot 'vcpkg.exe') install fftw3 ffmpeg libsamplerate libyaml eigen3 --triplet $Triplet
if ($LASTEXITCODE -ne 0) { throw "vcpkg install failed" }

# Configure and build ---------------------------------------------------------
$toolchain = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'

# Patch RogueVector for the modern MSVC STL --------------------------------
#
# RogueVector aliases externally-owned memory by reaching into std::vector's
# internals, with a separate implementation per standard library. Its OS_WIN32
# branch calls this->_Myfirst() / _Mylast() / _Myend() as member *functions*,
# which is how the MSVC STL exposed them up to Visual Studio 2013. They have
# been plain data members inside a private _Mypair ever since, so the branch no
# longer compiles - roughly 2000 errors, and the single largest obstacle to
# building Essentia here.
#
# Private members cannot be reached from the derived class, so this switches
# Windows to the same pointer-punning the file already uses for clang: the
# first three pointer-sized fields of std::vector are _Myfirst/_Mylast/_Myend
# (the allocator is empty and elided by the compressed pair). Just as
# unsupported as the clang path, and just as load-bearing.
#
# Patched in the working tree rather than committed to the submodule, exactly
# as build_essentia.sh does for its macOS fixes. Idempotent - re-runs are
# no-ops. Do not commit the result.
function Update-RogueVector {
    $file = Join-Path $essentiaRoot 'src\essentia\roguevector.h'
    if (-not (Test-Path $file)) { throw "roguevector.h not found at $file" }

    $content = Get-Content $file -Raw
    if ($content -notmatch [regex]::Escape('this->_Myfirst()')) {
        Write-Host "  . roguevector.h already patched"
        return
    }

    # Replaced statement by statement rather than as one block: the file is
    # checked out with core.autocrlf=true and ends up with mixed line endings,
    # which makes a multi-line literal match unreliable.
    $replacements = @(
        @{
            From = 'this->_Myfirst() = data;'
            To   = '*reinterpret_cast<T**>(this) = data;  // patched: modern MSVC STL has no _Myfirst()'
        },
        @{
            From = 'this->_Mylast() = this->_Myfirst() + size;'
            To   = "T** start = reinterpret_cast<T**>(this);`n  *(start + 1) = *start + size;"
        },
        @{
            From = 'this->_Myend() = this->_Myfirst() + size;'
            To   = '*(start + 2) = *start + size;'
        }
    )

    $updated = $content
    foreach ($replacement in $replacements) {
        if ($updated -notmatch [regex]::Escape($replacement.From)) {
            throw "Could not patch roguevector.h: expected to find '$($replacement.From)'. Check whether the Essentia submodule was updated."
        }
        $updated = $updated.Replace($replacement.From, $replacement.To)
    }

    Set-Content -Path $file -Value $updated -NoNewline
    Write-Host "  . roguevector.h: _Myfirst()/_Mylast()/_Myend() -> pointer punning (modern MSVC STL)"
}

Write-Host "-> Applying MSVC patches to the Essentia working tree"
Update-RogueVector

$installedPrefix = Join-Path $VcpkgRoot "installed\$Triplet"
if (-not (Test-Path (Join-Path $installedPrefix 'include'))) {
    throw "vcpkg reported success but $installedPrefix/include is missing"
}

Write-Host "-> Configuring Essentia ($Configuration)"
# CMAKE_PREFIX_PATH is passed explicitly rather than relying on the vcpkg
# toolchain to contribute it: find_path/find_library do not see the installed
# tree from the toolchain alone here, so plain (non-config) lookups fail.
& $cmakeExe -B $buildDir -S $cmakeSource `
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
    -DVCPKG_TARGET_TRIPLET=$Triplet `
    -DCMAKE_PREFIX_PATH="$installedPrefix" `
    -DAUDIONAUT_VCPKG_PREFIX="$installedPrefix" `
    -DESSENTIA_ROOT="$essentiaRoot"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

Write-Host "-> Building Essentia"
& $cmakeExe --build $buildDir --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "Essentia build failed" }

$lib = Get-ChildItem $buildDir -Recurse -Filter 'essentia.lib' | Select-Object -First 1
if (-not $lib) { throw "Build reported success but essentia.lib was not produced" }

Write-Host ""
Write-Host "Essentia build complete: $($lib.FullName)"
Write-Host "Size: $([math]::Round($lib.Length / 1MB, 1)) MB"
Write-Host ""
Write-Host "Configure the tests against it with:"
Write-Host "  cmake -B build -S Audionaut/Catch2Tests ``"
Write-Host "        -DCMAKE_TOOLCHAIN_FILE=$toolchain ``"
Write-Host "        -DVCPKG_TARGET_TRIPLET=$Triplet ``"
Write-Host "        -DAUDIONAUT_VCPKG_PREFIX=$installedPrefix"
