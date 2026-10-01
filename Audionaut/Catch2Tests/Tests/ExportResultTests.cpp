#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Export/AudioExporter.h"
#include "Engine/Export/ExportAudioConfig.h"
#include "Engine/PlayList/PlayListScheduler.h"

#include "TestUtils.h"
#include "TestEngine.h"

using namespace audium;
using namespace juce;

// The exporter's word has to be trustworthy: a bounce that could not be
// written reports so, and leaves nothing behind that could be mistaken for
// the result.
SCENARIO("export result honesty", "[engine][export]")
{
    auto inputFile = generateDcOffsetAudioFile(1.0);

    auto workDir = File::getSpecialLocation(File::tempDirectory).getChildFile("audionaut-export-tests");
    workDir.deleteRecursively();
    REQUIRE(workDir.createDirectory());

    auto config = std::make_shared<ExportAudioConfig>();
    config->fileName = workDir.getChildFile("bounce.wav");
    config->sampleRate = 44100.0;
    config->numChannels = 1;

    TestEngine engine;
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

    workDir.deleteRecursively();
    if (inputFile.existsAsFile())
        inputFile.deleteFile();
}

// Exporting twice in one session must give the same file twice: the first
// bounce must not leave state behind (a voice still fading out, a stale
// read-ahead) that bleeds into the start of the next one.
SCENARIO("a second bounce renders like the first", "[engine][export][rebounce]")
{
    auto inputFile = createRampAudioFile(1.0);

    auto workDir = File::getSpecialLocation(File::tempDirectory).getChildFile("audionaut-rebounce-tests");
    workDir.deleteRecursively();
    REQUIRE(workDir.createDirectory());

    TestEngine engine;
    engine->getProjectFileStore()->open(inputFile, nullptr);
    engine->getPlayListScheduler()->commitPlayListData();

    auto bounce = [&](const String& fileName) {
        auto config = std::make_shared<ExportAudioConfig>();
        config->fileName = workDir.getChildFile(fileName);
        config->sampleRate = 44100.0;
        config->numChannels = 1;
        config->bitDepth = 32;
        config->lengthSeconds = engine->getPlayListScheduler()->getTotalLength(audium::seconds);
        REQUIRE(AudioExporter(*engine, config).bounce());
        return audioFileToAudioBuffer(config->fileName);
    };

    GIVEN("the project bounced twice in a row")
    {
        auto first = bounce("first.wav");
        auto second = bounce("second.wav");

        THEN("both files hold the same samples")
        {
            REQUIRE(first.getNumSamples() == second.getNumSamples());

            auto differing = 0;
            auto firstDiffering = -1, lastDiffering = -1;
            for (auto s = 0; s < first.getNumSamples(); s++) {
                if (first.getSample(0, s) != second.getSample(0, s)) {
                    differing++;
                    if (firstDiffering < 0)
                        firstDiffering = s;
                    lastDiffering = s;
                }
            }
            INFO("differing samples: " << differing << " (from " << firstDiffering << " to " << lastDiffering << ")");
            String samples;
            for (auto s : { 0, 1, 2, 3, 10, 100, 500, 1000, 1022, 1023, 1024 })
                samples << "\n  " << s << ": " << first.getSample(0, s) << " vs " << second.getSample(0, s)
                        << " (second - first = " << (second.getSample(0, s) - first.getSample(0, s)) << ")";
            INFO(samples);
            REQUIRE(differing == 0);
        }
    }

    workDir.deleteRecursively();
    if (inputFile.existsAsFile())
        inputFile.deleteFile();
}
