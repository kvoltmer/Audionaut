#    Audionaut - Audio editing application for multitrack recordings.
#    Copyright (C) 2025 Klaus Voltmer
#
#    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.
#
# Locates the vcpkg-provided dependencies Essentia needs on Windows. Shared by
# Builds/Windows/CMakeLists.txt (which builds essentia.lib) and
# Catch2Tests/CMakeLists.txt (which links it), so the two cannot drift apart.
#
# Defines, after audionaut_find_essentia_deps():
#   ESSENTIA_DEP_INCLUDE_DIRS  include directories
#   ESSENTIA_DEP_LIBRARIES     libraries, per-configuration
#   ESSENTIA_DEP_DEFINITIONS   compile definitions required by those libraries

# vcpkg installs release libraries in <prefix>/lib and debug ones in
# <prefix>/debug/lib under the *same names*. A plain find_library caches
# whichever it happens to find first and then uses it for every configuration,
# which on a multi-config generator silently links a debug ffmpeg into a Release
# build - a CRT mismatch that only shows up as strange runtime behaviour. So
# resolve both and pick with a generator expression.
function(_audionaut_find_dual_library out_var)
    set(names ${ARGN})
    find_library(${out_var}_RELEASE NAMES ${names}
                 PATHS "${AUDIONAUT_VCPKG_PREFIX}/lib" NO_DEFAULT_PATH)
    find_library(${out_var}_DEBUG NAMES ${names}
                 PATHS "${AUDIONAUT_VCPKG_PREFIX}/debug/lib" NO_DEFAULT_PATH)

    if(NOT ${out_var}_RELEASE)
        message(FATAL_ERROR
            "Could not find ${names} in ${AUDIONAUT_VCPKG_PREFIX}/lib. "
            "Run Audionaut/Builds/build_essentia_windows.ps1 first.")
    endif()

    if(${out_var}_DEBUG)
        set(${out_var} "$<IF:$<CONFIG:Debug>,${${out_var}_DEBUG},${${out_var}_RELEASE}>"
            PARENT_SCOPE)
    else()
        set(${out_var} "${${out_var}_RELEASE}" PARENT_SCOPE)
    endif()
endfunction()

function(audionaut_find_essentia_deps)
    if(NOT AUDIONAUT_VCPKG_PREFIX)
        # Fall back to the first CMAKE_PREFIX_PATH entry, which is where the
        # vcpkg toolchain points when one was supplied.
        list(LENGTH CMAKE_PREFIX_PATH _prefix_count)
        if(_prefix_count GREATER 0)
            list(GET CMAKE_PREFIX_PATH 0 AUDIONAUT_VCPKG_PREFIX)
        endif()
    endif()
    if(NOT AUDIONAUT_VCPKG_PREFIX OR NOT EXISTS "${AUDIONAUT_VCPKG_PREFIX}/include")
        message(FATAL_ERROR
            "AUDIONAUT_VCPKG_PREFIX is not set to a vcpkg installed tree. "
            "Pass -DAUDIONAUT_VCPKG_PREFIX=<vcpkg>/installed/x64-windows-static-md.")
    endif()

    _audionaut_find_dual_library(FFTW3F_LIBRARY fftw3f)
    _audionaut_find_dual_library(SAMPLERATE_LIBRARY samplerate libsamplerate)
    _audionaut_find_dual_library(YAML_LIBRARY yaml libyaml)

    set(_ffmpeg_libs "")
    foreach(component avformat avcodec avutil swresample)
        _audionaut_find_dual_library(FFMPEG_${component}_LIBRARY ${component})
        list(APPEND _ffmpeg_libs "${FFMPEG_${component}_LIBRARY}")
    endforeach()

    # Eigen is header-only and lands under include/eigen3. Essentia includes it
    # unprefixed (<Eigen/Core>, <unsupported/Eigen/CXX11/Tensor>), so that
    # subdirectory itself has to be on the include path.
    set(_include_dirs
        "${AUDIONAUT_VCPKG_PREFIX}/include"
        "${AUDIONAUT_VCPKG_PREFIX}/include/eigen3")

    set(ESSENTIA_DEP_INCLUDE_DIRS "${_include_dirs}" PARENT_SCOPE)

    set(ESSENTIA_DEP_LIBRARIES
        "${FFTW3F_LIBRARY}"
        "${SAMPLERATE_LIBRARY}"
        ${_ffmpeg_libs}
        "${YAML_LIBRARY}"
        # FFmpeg's Windows backends. crypt32/ncrypt are pulled in by
        # tls_schannel.o (certificate and key handling), the rest by its
        # networking, DirectShow and Media Foundation code. None are linked by
        # a JUCE target by default.
        ws2_32
        secur32
        crypt32
        ncrypt
        bcrypt
        mfplat
        mfuuid
        strmiids
        PARENT_SCOPE)

    # libyaml's headers declare every entry point __declspec(dllimport) unless
    # YAML_DECLARE_STATIC is defined. Without it the references come out as
    # __imp_yaml_* and fail to resolve against the static yaml.lib.
    set(ESSENTIA_DEP_DEFINITIONS
        YAML_DECLARE_STATIC
        PARENT_SCOPE)
endfunction()
