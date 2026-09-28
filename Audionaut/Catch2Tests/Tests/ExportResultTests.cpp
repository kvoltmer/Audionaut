#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Export/AudioExporter.h"
#include "Engine/Export/ExportAudioConfig.h"
#include "Engine/PlayList/PlayListScheduler.h"

#include "TestUtils.h"

using namespace audium;
using namespace juce;

// The exporter's word has to be trustworthy: a bounce that could not be
// written reports so, and leaves nothing behind that could be mistaken for
// the result.
SCENARIO("export result honesty", "[engine][export]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto inputFile = generateDcOffsetAudioFile(1.0);

    auto workDir = File::getSpecialLocation(File::tempDirectory).getChildFile("audionaut-export-tests");
    workDir.deleteRecursively();
    REQUIRE(workDir.createDirectory());

    auto config = std::make_shared<ExportAudioConfig>();
    config->fileName = workDir.getChildFile("bounce.wav");
    config->sampleRate = 44100.0;
    config->numChannels = 1;

    auto engine = AudiumFactory::createAudiumEngine();
    engine->getProjectFileStore()->open(inputFile, nullptr);
    engine->getPlayListScheduler()->commitPlayListData();
    config->lengthSeconds = engine->getPlayListScheduler()->getTotalLength(audium::seconds);

    GIVEN("a bit depth the WAV writer rejects")
    {
        config->bitDepth = 12;

        WHEN("the project is bounced")
        {
            auto succeeded = AudioExporter(*engine, config).bounce();

            THEN("the bounce fails, names the bit depth, and writes no file")
            {
                REQUIRE_FALSE(succeeded);
                REQUIRE_FALSE(config->userCanceled);
                REQUIRE(config->failure == ExportAudioConfig::Failure::unsupportedFormat);
                REQUIRE(config->error.contains("12"));
                REQUIRE_FALSE(config->fileName.existsAsFile());
                REQUIRE(workDir.getNumberOfChildFiles(File::findFiles) == 0);
            }
        }

        WHEN("the target is a file left over from an earlier run")
        {
            REQUIRE(config->fileName.replaceWithText("stale"));
            auto succeeded = AudioExporter(*engine, config).bounce();

            THEN("the leftover does not turn the failure into a success")
            {
                REQUIRE_FALSE(succeeded);
                REQUIRE(config->error.isNotEmpty());
                REQUIRE(config->fileName.loadFileAsString() == "stale");
            }
        }
    }

    GIVEN("a target the output stream cannot be opened for")
    {
        config->bitDepth = 16;
        config->fileName = workDir.getChildFile("missing-directory").getChildFile("bounce.wav");

        WHEN("the project is bounced")
        {
            auto succeeded = AudioExporter(*engine, config).bounce();

            THEN("the bounce fails and names the path")
            {
                REQUIRE_FALSE(succeeded);
                REQUIRE(config->failure == ExportAudioConfig::Failure::cannotOpenOutput);
                REQUIRE(config->error.contains(config->fileName.getFullPathName()));
                REQUIRE_FALSE(config->fileName.existsAsFile());
            }
        }
    }

    GIVEN("a supported format")
    {
        config->bitDepth = 16;

        WHEN("the project is bounced")
        {
            auto succeeded = AudioExporter(*engine, config).bounce();

            THEN("the bounce succeeds and the file is there")
            {
                REQUIRE(succeeded);
                REQUIRE(config->failure == ExportAudioConfig::Failure::none);
                REQUIRE(config->error.isEmpty());
                REQUIRE(config->fileName.existsAsFile());
                REQUIRE(audioFileToAudioBuffer(config->fileName).getNumSamples() == 44100);
            }
        }

        WHEN("the bounce is split into mono files")
        {
            config->multiMono = true;
            auto succeeded = AudioExporter(*engine, config).bounce();

            THEN("the mono siblings replace the multichannel file")
            {
                REQUIRE(succeeded);
                REQUIRE_FALSE(config->fileName.existsAsFile());
                REQUIRE(workDir.getChildFile("bounce-01.wav").existsAsFile());
            }
        }
    }

    engine = nullptr;

    workDir.deleteRecursively();
    if (inputFile.existsAsFile())
        inputFile.deleteFile();

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}
