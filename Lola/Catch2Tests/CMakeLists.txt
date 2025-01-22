cmake_minimum_required(VERSION 3.15)

project(Lola_Tests VERSION 0.0.1)

set(APP_NAME "LolaTests")
message(STATUS "Name: ${APP_NAME}")

set(SUBMODULES_PATH "../../Submodules")
message(STATUS "Submodules at: ${SUBMODULES_PATH}")

set(SRC_PATH "../Source")
message(STATUS "source file root at: ${SRC_PATH}")


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
        ${SRC_PATH}/Engine/Undo/*.cpp
        ${SRC_PATH}/Engine/AudioSources/*.cpp
        ${SRC_PATH}/Engine/Export/*.cpp
        ${SRC_PATH}/Engine/Selection/*.cpp
        
        ${SRC_PATH}/Interface/*.cpp
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
        ${SRC_PATH}/Util/*.cpp
        
        ${SRC_PATH}/../JuceLibraryCode/BinaryData.cpp
        )
        
file(GLOB SRC_TESTS
        #JuceTest.cpp
        #FactorialTest.cpp
        ZoomHandlerTests.cpp
        ResourceContainerTests.cpp
        Tests/*.cpp
        )

target_sources(${APP_NAME}
        PRIVATE
        ${SRC_AUDIUM}
        ${SRC_TESTS})
        
        
target_include_directories(${APP_NAME}
                        PUBLIC ../Source/
                        PUBLIC ../../Submodules/json/include
                        PUBLIC ../../Submodules/farbot/include
                        )
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
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_console_app` call
        JUCE_USE_CURL=0)    # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_console_app` call


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


