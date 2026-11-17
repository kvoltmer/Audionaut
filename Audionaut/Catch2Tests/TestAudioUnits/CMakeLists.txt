# The plugin scan tests' Audio Units (macOS), and the worker process that
# scans them:
#
#   CrashOnScan.component   takes the process down when a host opens it
#   Empty.component         registers as an effect and has no plugin in it
#   CrashOnEditor.component a working pass-through effect that takes the
#                           process down when its editor is opened
#
# Both are complete component bundles, built beside the worker in
# <build>/test-audio-units-bin/AudionautTestScanWorker_artefacts/TestAudioUnits.
# The tests do not
# install them: AudionautTestScanWorker registers them in its own process
# (TestScanWorkerMain.cpp). To see what the app makes of them, copy a bundle
# to ~/Library/Audio/Plug-Ins/Components - and take it out again afterwards,
# every host on the machine sees it there.
#
# Added from ../CMakeLists.txt, after JUCE and the scan helper.

set(TEST_AU_SRC "${CMAKE_CURRENT_LIST_DIR}/../../Source")
set(TEST_AU_SUBMODULES "${CMAKE_CURRENT_LIST_DIR}/../../../Submodules")

juce_add_console_app(AudionautTestScanWorker)
juce_generate_juce_header(AudionautTestScanWorker)

# the worker's files, as the scan helper has them (../../ScanHelper/CMakeLists.txt)
target_sources(AudionautTestScanWorker
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/TestScanWorkerMain.cpp
        ${TEST_AU_SRC}/Engine/Effects/Hosting/ChildProcessScanner.cpp
        ${TEST_AU_SRC}/Engine/Effects/Hosting/PluginFormats.cpp
        ${TEST_AU_SRC}/Engine/Effects/Hosting/ScanWorker.cpp)

target_include_directories(AudionautTestScanWorker
    PRIVATE
        ${TEST_AU_SRC}
        ${TEST_AU_SUBMODULES}/json/include)

set_property(TARGET AudionautTestScanWorker PROPERTY CXX_STANDARD 20)

target_compile_definitions(AudionautTestScanWorker
    PRIVATE
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_PLUGINHOST_AU=1
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_LV2=1)

target_link_libraries(AudionautTestScanWorker
    PRIVATE
        juce::juce_audio_processors
        "-framework CoreAudioKit"
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Apple's Audio Unit SDK, which JUCE brings for the plugins it builds: what a
# test component that has to work as an effect is made of.
set(TEST_AU_SDK "${TEST_AU_SUBMODULES}/JUCE/modules/juce_audio_plugin_client/AU")
add_library(audionaut_test_ausdk STATIC
    ${TEST_AU_SDK}/AudioUnitSDK/AUBase.cpp
    ${TEST_AU_SDK}/AudioUnitSDK/AUBuffer.cpp
    ${TEST_AU_SDK}/AudioUnitSDK/AUBufferAllocator.cpp
    ${TEST_AU_SDK}/AudioUnitSDK/AUEffectBase.cpp
    ${TEST_AU_SDK}/AudioUnitSDK/AUInputElement.cpp
    ${TEST_AU_SDK}/AudioUnitSDK/AUOutputElement.cpp
    ${TEST_AU_SDK}/AudioUnitSDK/AUPlugInDispatch.cpp
    ${TEST_AU_SDK}/AudioUnitSDK/AUScopeElement.cpp
    ${TEST_AU_SDK}/AudioUnitSDK/ComponentBase.cpp)
target_include_directories(audionaut_test_ausdk SYSTEM PUBLIC ${TEST_AU_SDK})
target_compile_options(audionaut_test_ausdk PRIVATE -w)   # not ours to tidy
set_target_properties(audionaut_test_ausdk PROPERTIES CXX_STANDARD 20 POSITION_INDEPENDENT_CODE ON)

# One component bundle: its four-character subtype, the name it is listed
# under ("Audionaut Tests: <name>") and the factory its source exports;
# after those, the libraries it links besides AudioToolbox.
function(audionaut_test_audio_unit target subtype name factory source)
    add_library(${target} MODULE ${source})
    target_link_libraries(${target} PRIVATE ${ARGN})

    set(TEST_AU_EXECUTABLE ${target})
    set(TEST_AU_SUBTYPE ${subtype})
    set(TEST_AU_NAME ${name})
    set(TEST_AU_FACTORY ${factory})
    configure_file(${CMAKE_CURRENT_LIST_DIR}/Info.plist.in ${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist @ONLY)

    set_target_properties(${target} PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION component
        OUTPUT_NAME ${target}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist
        LIBRARY_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:AudionautTestScanWorker>/TestAudioUnits"
        CXX_STANDARD 20)
    target_link_libraries(${target} PRIVATE "-framework AudioToolbox")

    # the worker is of no use without them
    add_dependencies(AudionautTestScanWorker ${target})
endfunction()

audionaut_test_audio_unit(CrashOnScan Cras "Crash On Scan" AudionautTestCrashOnScanFactory CrashOnScan.cpp)
audionaut_test_audio_unit(Empty       Empt "Empty"         AudionautTestEmptyFactory       Empty.cpp)
audionaut_test_audio_unit(CrashOnEditor CrEd "Crash On Editor" CrashOnEditorFactory CrashOnEditor.mm
                          audionaut_test_ausdk "-framework Cocoa" "-framework AudioUnit" "-framework CoreAudio")
