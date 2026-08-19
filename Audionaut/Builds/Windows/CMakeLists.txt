#    Audionaut - Audio editing application for multitrack recordings.
#    Copyright (C) 2025 Klaus Voltmer
#
#    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.
#
# Builds Essentia as a static library with MSVC, replacing its waf build, which
# does not work on Windows. Dependencies come from vcpkg rather than Essentia's
# packaging/ scripts (bash + MinGW, producing archives MSVC cannot link).
#
# Not invoked directly - use Audionaut/Builds/build_essentia_windows.ps1, which
# installs the vcpkg dependencies and passes the toolchain file.
#
# The Essentia submodule is never modified: generated sources go to the build
# directory and the one POSIX gap is bridged by compat/unistd.h.

cmake_minimum_required(VERSION 3.21)

project(essentia_msvc LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ESSENTIA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../Submodules/essentia"
    CACHE PATH "Path to the Essentia submodule")
get_filename_component(ESSENTIA_ROOT "${ESSENTIA_ROOT}" ABSOLUTE)
set(ESSENTIA_SRC "${ESSENTIA_ROOT}/src")

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Regenerate whenever the generator or Essentia's algorithm headers change.
# Run at configure time because the result *is* the source list.
set(ESSENTIA_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
execute_process(
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_LIST_DIR}/generate_essentia_build.py"
            "${ESSENTIA_ROOT}" "${ESSENTIA_GEN_DIR}"
    RESULT_VARIABLE ESSENTIA_GEN_RESULT
    OUTPUT_VARIABLE ESSENTIA_GEN_OUTPUT
    ERROR_VARIABLE  ESSENTIA_GEN_ERROR)
if(NOT ESSENTIA_GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "Essentia source generation failed:\n${ESSENTIA_GEN_OUTPUT}\n${ESSENTIA_GEN_ERROR}")
endif()
message(STATUS "${ESSENTIA_GEN_OUTPUT}")

include("${ESSENTIA_GEN_DIR}/essentia_sources.cmake")

# Dependencies are located by VcpkgDeps.cmake, shared with
# Catch2Tests/CMakeLists.txt so the two cannot drift. find_package is avoided
# deliberately: the vcpkg config-package names are inconsistent across these
# ports (fftw3 exports FFTW3f, libsamplerate exports SampleRate, libyaml and
# ffmpeg export nothing usable) and have changed between vcpkg releases.
include("${CMAKE_CURRENT_LIST_DIR}/VcpkgDeps.cmake")
audionaut_find_essentia_deps()

add_library(essentia STATIC
    ${ESSENTIA_CORE_SOURCES}
    ${ESSENTIA_ALGORITHM_SOURCES}
    ${ESSENTIA_3RDPARTY_SOURCES}
    ${ESSENTIA_GENERATED_SOURCES})

target_include_directories(essentia PUBLIC
    # compat/ first: it supplies the <unistd.h> that debugging.h wants.
    "${CMAKE_CURRENT_LIST_DIR}/compat"
    "${ESSENTIA_GEN_DIR}"          # version.h
    "${ESSENTIA_SRC}"
    "${ESSENTIA_SRC}/essentia"
    "${ESSENTIA_SRC}/essentia/scheduler"
    "${ESSENTIA_SRC}/essentia/streaming"
    "${ESSENTIA_SRC}/essentia/streaming/algorithms"
    "${ESSENTIA_SRC}/essentia/utils"
    "${ESSENTIA_SRC}/3rdparty"
    "${ESSENTIA_SRC}/3rdparty/spline"
    "${ESSENTIA_SRC}/3rdparty/nnls"
    "${ESSENTIA_SRC}/3rdparty/cephes/bessel"
    ${ESSENTIA_DEP_INCLUDE_DIRS})

# OS_WIN32 is deliberately NOT defined here: src/essentia/config.h already
# derives it from _MSC_VER/_WIN32, which is what selects the Win32 branch of
# threading.h (a CRITICAL_SECTION ForcedMutex instead of pthreads). Defining it
# on the command line as well only produces C4005 macro-redefinition warnings.
target_compile_definitions(essentia PUBLIC
    # Both from src/wscript.
    __STDC_CONSTANT_MACROS
    EIGEN_MPL2_ONLY
    EIGEN_PERMANENTLY_DISABLE_STUPID_WARNINGS
    # Essentia calls plenty of functions MSVC deprecates in favour of its _s
    # variants, and uses M_PI and friends from <cmath>.
    _CRT_SECURE_NO_WARNINGS
    _USE_MATH_DEFINES
    NOMINMAX
    # windows.h (dragged in by threading.h) otherwise includes winsock.h, which
    # collides with the winsock2.h that Ableton Link's asio needs downstream.
    WIN32_LEAN_AND_MEAN
    ${ESSENTIA_DEP_DEFINITIONS})

target_link_libraries(essentia PUBLIC ${ESSENTIA_DEP_LIBRARIES})

if(MSVC)
    # /MP: compile this target's translation units in parallel. MSBuild's -m
    # (what cmake --build --parallel maps to) only parallelizes across
    # *projects*, and essentia is a single project with ~800 files - without
    # /MP they all go through one core, which timed the CI job out.
    #
    # /bigobj: several algorithm translation units exceed the default section
    # limit once Eigen templates are instantiated.
    #
    # /FIiso646.h: humdetector.cpp uses the alternative operator token "not".
    # MSVC does not treat those as keywords unless /permissive- is on, and
    # force-including <iso646.h> supplies them as macros without switching the
    # whole (pre-C++11-era) codebase to strict conformance mode.
    target_compile_options(essentia PRIVATE /MP /bigobj /W0 /FIiso646.h)
endif()

# Land the archive at build-msvc/lib/<config>/essentia.lib, which is where
# Audionaut/Catch2Tests/CMakeLists.txt looks for it. Multi-config generators
# append the configuration directory themselves.
set_target_properties(essentia PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

install(TARGETS essentia ARCHIVE DESTINATION lib)
