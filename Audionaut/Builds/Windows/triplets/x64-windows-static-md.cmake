#    Audionaut - Audio editing application for multitrack recordings.
#    Copyright (C) 2025 Klaus Voltmer
#
#    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.
#
# Overlay triplet for CI: vcpkg's stock x64-windows-static-md (static
# libraries against the dynamic CRT) plus VCPKG_BUILD_TYPE=release. CI only
# links the Release configuration, and building the Debug flavour of the
# dependencies as well - ffmpeg above all - roughly doubles a cold dependency
# build, which is what pushed the Windows job past CircleCI's 60-minute job
# limit.
#
# Selected via build_essentia_windows.ps1 -ReleaseOnlyDeps. Local builds that
# need Debug dependencies omit the switch and get the stock triplet.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)
