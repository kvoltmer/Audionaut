cmake_minimum_required(VERSION 3.15)

project(Audionaut_Tests VERSION 0.0.1)

# Required for the add_test() at the bottom to reach CTest. Without it CMake
# writes no CTestTestfile.cmake, add_test() is silently discarded, and `ctest`
# exits 0 in about a second having found nothing - a green CI run that never
# executed a single assertion.
enable_testing()

set(APP_NAME "AudionautTests")
message(STATUS "Name: ${APP_NAME}")

set(SUBMODULES_PATH "${CMAKE_CURRENT_LIST_DIR}/../../Submodules")
message(STATUS "Submodules at: ${SUBMODULES_PATH}")

set(SRC_PATH "${CMAKE_CURRENT_LIST_DIR}/../Source")
message(STATUS "source file root at: ${SRC_PATH}")

# Essentia powers the audio-analysis features. It is not produced by this CMake
# build - build it from the submodule with Audionaut/Builds/build_essentia.sh.
# When AUDIONAUT_ENABLE_ESSENTIA is OFF the analysis code compiles to no-ops (see
# ESSENTIA_ENABLED in the segmenters) and the Essentia headers/libs are skipped.
#
# Link against the freshly built libs in the submodule build tree:
#   - libessentia.a under essentia/build/src
#   - its 3rd-party static deps under essentia/packaging/debian_3rdparty/lib
# Default to whatever is actually available: require both the Eigen headers (a
# build artifact absent from a clean submodule checkout) and the built
# libessentia.a. Override with -DAUDIONAUT_ENABLE_ESSENTIA=ON/OFF.
set(ESSENTIA_LIB_PATH "${SUBMODULES_PATH}/essentia/build/src")
set(ESSENTIA_3RDPARTY_LIB_PATH "${SUBMODULES_PATH}/essentia/packaging/debian_3rdparty/lib")

# Windows/MSVC gets Essentia from a different build entirely - CMake + vcpkg
# instead of waf + packaging/, because waf has no working Windows path. See
# Audionaut/Builds/Windows/README.md. That build lays things out differently:
# essentia.lib under build-msvc/lib/<config>/, dependencies from vcpkg, and the
# generated version.h in build-msvc/generated. Produce it with
# Audionaut/Builds/build_essentia_windows.ps1.
set(ESSENTIA_MSVC_ROOT "${SUBMODULES_PATH}/essentia/build-msvc")
if(MSVC)
    # Multi-config generator: the configuration is not known until build time,
    # so accept either and pick per-config with a generator expression below.
    if(EXISTS "${ESSENTIA_MSVC_ROOT}/lib/Release/essentia.lib"
       OR EXISTS "${ESSENTIA_MSVC_ROOT}/lib/Debug/essentia.lib")
        set(ESSENTIA_AVAILABLE ON)
    else()
        set(ESSENTIA_AVAILABLE OFF)
    endif()
elseif(EXISTS "${SUBMODULES_PATH}/essentia/packaging/debian_3rdparty/include/eigen3/signature_of_eigen3_matrix_library"
       AND EXISTS "${ESSENTIA_LIB_PATH}/libessentia.a")
    set(ESSENTIA_AVAILABLE ON)
else()
    set(ESSENTIA_AVAILABLE OFF)
endif()
option(AUDIONAUT_ENABLE_ESSENTIA "Build against the Essentia library" ${ESSENTIA_AVAILABLE})
message(STATUS "Essentia enabled: ${AUDIONAUT_ENABLE_ESSENTIA} (detected available: ${ESSENTIA_AVAILABLE})")
message(STATUS "Essentia libs at: ${ESSENTIA_LIB_PATH} / ${ESSENTIA_3RDPARTY_LIB_PATH}")

# Code coverage. Off by default; the instrumentation is attached to the
# AudionautTests target only (see below), so JUCE, Catch2 and the other
# submodules are never instrumented and never appear in the report.
#
#   cmake -B build-coverage -S Audionaut/Catch2Tests \
#         -DCMAKE_BUILD_TYPE=Debug -DAUDIONAUT_ENABLE_COVERAGE=ON
#   cmake --build build-coverage -j8 --target coverage
option(AUDIONAUT_ENABLE_COVERAGE "Instrument the test target for code coverage" OFF)
message(STATUS "Coverage enabled: ${AUDIONAUT_ENABLE_COVERAGE}")

# If you've installed JUCE somehow (via a package manager, or directly using the CMake install
# target), you'll need to tell this project that it depends on the installed copy of JUCE. If you've
# included JUCE directly in your source tree (perhaps as a submodule), you'll need to tell CMake to
# include that subdirectory as part of the build.

# find_package(JUCE CONFIG REQUIRED)        # If you've installed JUCE to your system
# or

add_subdirectory(${SUBMODULES_PATH}/JUCE juce-bin)                    # If you've put JUCE in a subdirectory called JUCE

# Catch2
#find_package(Catch2)

add_subdirectory(${SUBMODULES_PATH}/Catch2 catch2-bin)

# `juce_add_console_app` adds an executable target with the name passed as the first argument
# (ConsoleAppExample here). This target is a normal CMake target, but has a lot of extra properties
# set up by default. This function accepts many optional arguments. Check the readme at
# `docs/CMake API.md` in the JUCE repo for the full list.

juce_add_console_app(${APP_NAME} PRODUCT_NAME ${APP_NAME})     # The name of the final executable, which can differ from the target name

# `juce_generate_juce_header` will create a JuceHeader.h for a given target, which will be generated
# into the build tree. This header should be included with `#include <JuceHeader.h>`. The include
# path for this header will be automatically added to the target. The main function of the
# JuceHeader is to include all the JUCE module headers for a particular target; if you're happy to
# include module headers directly, you probably don't need to call this.

juce_generate_juce_header(${APP_NAME})


file(GLOB SRC_AUDIUM
        ${SRC_PATH}/Engine/*.cpp
        ${SRC_PATH}/Engine/Analysis/*.cpp
        ${SRC_PATH}/Engine/Core/*.cpp
        ${SRC_PATH}/Engine/Channel/*.cpp
        ${SRC_PATH}/Engine/AutoEdit/*.cpp
        ${SRC_PATH}/Engine/Channel/*.cpp
        ${SRC_PATH}/Engine/Factory/*.cpp
        ${SRC_PATH}/Engine/Group/*.cpp
        ${SRC_PATH}/Engine/Link/*.cpp
        ${SRC_PATH}/Engine/Playback/*.cpp
        ${SRC_PATH}/Engine/PlayList/*.cpp
        ${SRC_PATH}/Engine/Provider/*.cpp
        ${SRC_PATH}/Engine/Region/*.cpp
        ${SRC_PATH}/Engine/Resource/*.cpp
        ${SRC_PATH}/Engine/Recording/*.cpp
        ${SRC_PATH}/Engine/Undo/*.cpp
        ${SRC_PATH}/Engine/AudioSources/*.cpp
        ${SRC_PATH}/Engine/Export/*.cpp
        ${SRC_PATH}/Engine/Selection/*.cpp
        
        ${SRC_PATH}/Interface/*.cpp
        ${SRC_PATH}/Interface/LookAndFeel/*.cpp
        ${SRC_PATH}/Interface/Components/*.cpp
        ${SRC_PATH}/Interface/Components/HeaderPanel/*.cpp
        ${SRC_PATH}/Interface/Components/MiddlePanel/*.cpp
        ${SRC_PATH}/Interface/Components/MiddlePanel/ArrangementView/*.cpp
        ${SRC_PATH}/Interface/Components/MiddlePanel/ChannelView/*.cpp
        ${SRC_PATH}/Interface/Components/MiddlePanel/EditView/*.cpp
        ${SRC_PATH}/Interface/Components/RightPanel/*.cpp
        ${SRC_PATH}/Interface/Controls/*.cpp
        ${SRC_PATH}/Interface/Dialogs/*.cpp
        ${SRC_PATH}/Interface/Models/*.cpp
        ${SRC_PATH}/Interface/Handlers/*.cpp
        ${SRC_PATH}/Interface/Views/*.cpp
        ${SRC_PATH}/Interface/Widgets/*.cpp
        ${SRC_PATH}/Interface/DragAndDrop/*.cpp
        ${SRC_PATH}/Util/*.cpp
        )
        
file(GLOB SRC_TESTS
        Tests/*.cpp
        )

target_sources(${APP_NAME}
        PRIVATE
        ${SRC_AUDIUM}
        ${SRC_TESTS}
        ${SRC_PATH}/../JuceLibraryCode/BinaryData.cpp)
        
# The source_group function creates a group in the IDE project for the files passed as arguments.
source_group(TREE "${SRC_PATH}" PREFIX "Source Files" FILES ${SRC_AUDIUM})


target_include_directories(${APP_NAME}
                        PUBLIC ../Source/
                        PUBLIC ${SUBMODULES_PATH}/json/include
                        PUBLIC ${SUBMODULES_PATH}/farbot/include
                        )

if(AUDIONAUT_ENABLE_ESSENTIA)
    if(MSVC)
        # Eigen comes from vcpkg rather than Essentia's packaging/ tree, and
        # version.h is generated into the build dir instead of into src/.
        target_include_directories(${APP_NAME}
                            PUBLIC ${SUBMODULES_PATH}/essentia/src
                            PUBLIC ${ESSENTIA_MSVC_ROOT}/generated
                            PUBLIC ${AUDIONAUT_VCPKG_PREFIX}/include
                            PUBLIC ${AUDIONAUT_VCPKG_PREFIX}/include/eigen3
                            )
        # Pulling in Essentia's headers drags <windows.h> into this target
        # (threading.h includes it for the CRITICAL_SECTION mutex), and its
        # defaults break two other dependencies:
        #
        #   NOMINMAX            - windows.h defines min/max as macros, which
        #                         wrecks the std::min/std::max calls in
        #                         Essentia's own devnull.h.
        #   WIN32_LEAN_AND_MEAN - stops windows.h including winsock.h, which
        #                         otherwise collides with the winsock2.h that
        #                         Ableton Link's bundled asio requires
        #                         ("WinSock.h has already been included").
        target_compile_definitions(${APP_NAME} PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    else()
        target_include_directories(${APP_NAME}
                            PUBLIC ${SUBMODULES_PATH}/essentia/src
                            PUBLIC ${SUBMODULES_PATH}/essentia/packaging/debian_3rdparty/include/eigen3
                            )
    endif()
endif()
# c++ standard
#set(CMAKE_CXX_STANDARD 20)
set_property(TARGET ${APP_NAME} PROPERTY CXX_STANDARD 20)

# `target_compile_definitions` adds some preprocessor definitions to our target. In a Projucer
# project, these might be passed in the 'Preprocessor Definitions' field. JUCE modules also make use
# of compile definitions to switch certain features on/off, so if there's a particular feature you
# need that's not on by default, check the module header for the correct flag to set here. These
# definitions will be visible both to your code, and also the JUCE module code, so for new
# definitions, pick unique names that are unlikely to collide! This is a standard CMake command.

target_compile_definitions(${APP_NAME}
    PRIVATE
        JUCE_MODAL_LOOPS_PERMITTED=1
        CATCH2_TESTS=1
        CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" # This is needed for the Catch2 tests to find the test files
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_console_app` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_console_app` call
        ESSENTIA_ENABLED=$<BOOL:${AUDIONAUT_ENABLE_ESSENTIA}>)


# ---------------------------------------------------------------------------
# Coverage instrumentation
#
# Scoped to this target with target_compile_options/target_link_options rather
# than CMAKE_CXX_FLAGS: a global flag would also instrument JUCE, Catch2, Link
# and Essentia, which multiplies build time and drowns the report in submodule
# source we don't own.
# ---------------------------------------------------------------------------
if(AUDIONAUT_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR
            "AUDIONAUT_ENABLE_COVERAGE requires GCC or Clang (got ${CMAKE_CXX_COMPILER_ID}).")
    endif()

    # Line counts are only meaningful without inlining, and JUCE's
    # juce_recommended_config_flags injects -O3 for non-Debug builds through its
    # INTERFACE options, which land after ours on the command line and would win.
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING
            "AUDIONAUT_ENABLE_COVERAGE is ON but CMAKE_BUILD_TYPE is "
            "'${CMAKE_BUILD_TYPE}'. Optimised builds inline away lines and skew "
            "the report - configure with -DCMAKE_BUILD_TYPE=Debug.")
    endif()

    target_compile_options(${APP_NAME} PRIVATE -O0 -g --coverage)
    target_link_options(${APP_NAME} PRIVATE --coverage)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # The engine runs work on real-time audio threads; non-atomic counter
        # updates race there and silently under-count. Costs some run time.
        target_compile_options(${APP_NAME} PRIVATE -fprofile-update=atomic)
    endif()

    # `coverage` target: run the suite, then summarise. gcovr is optional - the
    # instrumented build is still usable with plain gcov without it.
    find_program(GCOVR_EXECUTABLE gcovr)
    if(GCOVR_EXECUTABLE)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            set(GCOVR_GCOV_ARG --gcov-executable "llvm-cov gcov")
        else()
            set(GCOVR_GCOV_ARG "")
        endif()

        # --root is the repo root, not SRC_PATH: gcovr resolves --filter against
        # it, and rooting at SRC_PATH filters every file back out again.
        # --gcov-ignore-errors=no_working_dir_found is required, not defensive:
        # some Essentia headers pulled into the analysis TUs defeat gcovr's
        # working-directory inference and abort the whole run without it.
        # Both paths must be normalised: SRC_PATH is spelled ".../Catch2Tests/../Source"
        # and gcovr matches --filter as a literal path prefix without resolving
        # "..", so the unnormalised form silently filters out every file.
        get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
        get_filename_component(COVERAGE_SRC_ROOT "${SRC_PATH}" ABSOLUTE)

        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage-html
            COMMAND $<TARGET_FILE:${APP_NAME}>
            COMMAND ${GCOVR_EXECUTABLE}
                    --root ${REPO_ROOT}
                    --filter ${COVERAGE_SRC_ROOT}
                    ${GCOVR_GCOV_ARG}
                    --gcov-ignore-errors=no_working_dir_found
                    --exclude-unreachable-branches
                    --print-summary
                    --xml ${CMAKE_BINARY_DIR}/coverage.xml
                    --html-details ${CMAKE_BINARY_DIR}/coverage-html/index.html
                    .
            DEPENDS ${APP_NAME}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running AudionautTests and generating coverage report"
            VERBATIM)
    else()
        message(STATUS
            "gcovr not found - the 'coverage' target is unavailable. "
            "Install it (pipx install gcovr) or read the .gcda files with gcov directly.")
    endif()
endif()


#target_compile_options( ${APP_NAME}
#    PRIVATE
#        -Wall -Wcast-align -Wfloat-equal -Wno-ignored-qualifiers -Wsign-compare -Wsign-conversion -Wstrict-aliasing -Wswitch-enum -Wuninitialized -Wunreachable-code -Wunused-parameter -Wmissing-field-initializers -Wshadow-all -Wshorten-64-to-32 -Wconversion -Wint-conversion -Wconditional-uninitialized -Wconstant-conversion -Wbool-conversion -Wextra-semi -Wshift-sign-overflow -Wmissing-prototypes -Wnullable-to-nonnull-conversion -Wpedantic -Wdeprecated -Woverloaded-virtual -Wreorder -Wzero-as-null-pointer-constant -Wunused-private-field -Winconsistent-missing-destructor-override -Wshadow-field-in-constructor "-fsigned-char" )

# If the target needs extra binary assets, they can be added here. The first argument is the name of
# a new static library target that will include all the binary resources. There is an optional
# `NAMESPACE` argument that can specify the namespace of the generated binary data class. Finally,
# the SOURCES argument should be followed by a list of source files that should be built into the
# static library. These source files can be of any kind (wav data, images, fonts, icons etc.).
# Conversion to binary-data will happen when the target is built.

# juce_add_binary_data(ConsoleAppData SOURCES ...)

# `target_link_libraries` links libraries and JUCE modules to other libraries or executables. Here,
# we're linking our executable target to the `juce::juce_core` module. Inter-module dependencies are
# resolved automatically. If you'd generated a binary data target above, you would need to link to
# it here too. This is a standard CMake command.

# Ableton Link
include(${SUBMODULES_PATH}/link/AbletonLinkConfig.cmake)
#target_link_libraries(${APP_NAME} Ableton::Link)


target_link_libraries(${APP_NAME}
    PRIVATE
        # ConsoleAppData            # If you'd created a binary data target, you'd link to it here
        juce::juce_core
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_dsp
        Catch2::Catch2WithMain
        Ableton::Link
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(AUDIONAUT_ENABLE_ESSENTIA AND MSVC)
    # MSVC's linker resolves archives by symbol table rather than left to right,
    # so the ordering the POSIX branch below needs does not apply here.
    #
    # The dependency list is shared with the Essentia build itself via
    # VcpkgDeps.cmake, so the library set, the debug/release selection and the
    # required definitions cannot drift between the two. Configuring this build
    # therefore needs the vcpkg prefix - build_essentia_windows.ps1 prints the
    # exact command when it finishes.
    include("${CMAKE_CURRENT_LIST_DIR}/../Builds/Windows/VcpkgDeps.cmake")
    audionaut_find_essentia_deps()

    target_compile_definitions(${APP_NAME} PUBLIC ${ESSENTIA_DEP_DEFINITIONS})

    target_link_libraries(${APP_NAME}
        PRIVATE
            "${ESSENTIA_MSVC_ROOT}/lib/$<CONFIG>/essentia.lib"
            ${ESSENTIA_DEP_LIBRARIES})
elseif(AUDIONAUT_ENABLE_ESSENTIA)
    target_link_libraries(${APP_NAME}
        PRIVATE
            # GNU ld resolves static libs left to right, so dependents must
            # precede their dependencies: essentia/chromaprint -> avformat ->
            # avcodec -> swresample -> avutil, codecs and utility libs last.
            ${ESSENTIA_LIB_PATH}/libessentia.a
            ${ESSENTIA_3RDPARTY_LIB_PATH}/libchromaprint.a
            ${ESSENTIA_3RDPARTY_LIB_PATH}/libavformat.a
            ${ESSENTIA_3RDPARTY_LIB_PATH}/libavcodec.a
            ${ESSENTIA_3RDPARTY_LIB_PATH}/libswresample.a
            ${ESSENTIA_3RDPARTY_LIB_PATH}/libavutil.a
            ${ESSENTIA_3RDPARTY_LIB_PATH}/libmp3lame.a
            ${ESSENTIA_3RDPARTY_LIB_PATH}/libfftw3f.a
            ${ESSENTIA_3RDPARTY_LIB_PATH}/libz.a
            ${ESSENTIA_3RDPARTY_LIB_PATH}/libsamplerate.a
            ${ESSENTIA_3RDPARTY_LIB_PATH}/libyaml.a)
    if(APPLE)
        # glibc provides iconv natively; a separate libiconv exists only on macOS
        target_link_libraries(${APP_NAME} PRIVATE libiconv.a)
    else()
        # libavutil's hwcontext_drm needs libdrm on Linux
        target_link_libraries(${APP_NAME} PRIVATE drm)
    endif()
endif()


add_test(NAME ${APP_NAME}
         COMMAND ${APP_NAME})

set_target_properties(${APP_NAME}
                      PROPERTIES
                      XCODE_ATTRIBUTE_DEVELOPMENT_TEAM Q89CZBX79G)
                    



