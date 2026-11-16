cmake_minimum_required(VERSION 3.15)

# audionaut-scan-helper: the plugin scan worker as an executable of its own,
# for the sandboxed macOS app, which cannot run its own binary as a child
# process (see ScanHelperMain.cpp). The app is built by the Projucer's Xcode
# project; its post-build step (embed-scan-helper.sh) builds this project
# and puts the signed helper into the bundle.
#
# Stand-alone:
#   cmake -B build-scan-helper -S Audionaut/ScanHelper
#   cmake --build build-scan-helper
#
# The test build (Catch2Tests/CMakeLists.txt) adds this directory as well,
# so that CI compiles the helper and runs it on a real plugin; JUCE is then
# already there.
project(Audionaut_ScanHelper VERSION 0.0.1)

set(SCAN_HELPER_SUBMODULES "${CMAKE_CURRENT_LIST_DIR}/../../Submodules")
set(SCAN_HELPER_SRC "${CMAKE_CURRENT_LIST_DIR}/../Source")

if(NOT TARGET juce::juce_core)
    add_subdirectory(${SCAN_HELPER_SUBMODULES}/JUCE juce-bin)
endif()

juce_add_console_app(AudionautScanHelper)
set_target_properties(AudionautScanHelper PROPERTIES OUTPUT_NAME audionaut-scan-helper)

# The helper's JuceHeader.h, with the few modules it links. Sources include
# it with `#include <JuceHeader.h>`.
juce_generate_juce_header(AudionautScanHelper)

# Only what the worker is made of - nothing of the engine, which keeps the
# helper small and its build short. A file added to the worker's side goes
# in here.
target_sources(AudionautScanHelper
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/ScanHelperMain.cpp
        ${SCAN_HELPER_SRC}/Engine/Effects/Hosting/ChildProcessScanner.cpp
        ${SCAN_HELPER_SRC}/Engine/Effects/Hosting/PluginFormats.cpp
        ${SCAN_HELPER_SRC}/Engine/Effects/Hosting/ScanWorker.cpp)

target_include_directories(AudionautScanHelper
    PRIVATE
        ${SCAN_HELPER_SRC}
        ${SCAN_HELPER_SUBMODULES}/json/include)

set_property(TARGET AudionautScanHelper PROPERTY CXX_STANDARD 20)

if(MSVC)
    target_compile_definitions(AudionautScanHelper PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(AudionautScanHelper PRIVATE /MP)
endif()

target_compile_definitions(AudionautScanHelper
    PRIVATE
        JUCE_MODAL_LOOPS_PERMITTED=1   # the worker pumps the message loop while a plugin loads
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        # the same three as the app: which formats a process hosts is chosen
        # at runtime (PluginFormats::addPlatformFormats)
        JUCE_PLUGINHOST_AU=1
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_LV2=1)

target_link_libraries(AudionautScanHelper
    PRIVATE
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(APPLE)
    # AUGenericView, which the Audio Unit format refers to; juce_audio_utils
    # would bring the framework, and with it the audio device and file
    # format modules the helper has no use for.
    target_link_libraries(AudionautScanHelper PRIVATE "-framework CoreAudioKit")
endif()
