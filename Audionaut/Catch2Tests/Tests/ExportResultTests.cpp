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

// FLAC is lossless: a FLAC bounce holds the very samples a WAV bounce of the
// same depth does. The format's limits (16/24 bit, eight channels per file)
// are refused up front, like any other unsupported format.
SCENARIO("FLAC export", "[engine][export][flac]")
{
    auto inputFile = createRampAudioFile(1.0);

    auto workDir = File::getSpecialLocation(File::tempDirectory).getChildFile("audionaut-flac-export-tests");
    workDir.deleteRecursively();
    REQUIRE(workDir.createDirectory());

    TestEngine engine;
    engine->getProjectFileStore()->open(inputFile, nullptr);
    engine->getPlayListScheduler()->commitPlayListData();

    auto makeConfig = [&](const String& fileName, ExportFormat format) {
        auto config = std::make_shared<ExportAudioConfig>();
        config->fileName = workDir.getChildFile(fileName);
        config->format = format;
        config->sampleRate = 44100.0;
        config->numChannels = 1;
        config->bitDepth = 24;
        config->lengthSeconds = engine->getPlayListScheduler()->getTotalLength(audium::seconds);
        return config;
    };

    GIVEN("a 24-bit FLAC bounce of a ramp")
    {
        auto flac = makeConfig("bounce.flac", ExportFormat::flac);
        REQUIRE(AudioExporter(*engine, flac).bounce());

        THEN("the file is a 24-bit FLAC holding the ramp to within 24-bit quantisation")
        {
            AudioFormatManager formatManager;
            formatManager.registerBasicFormats();
            std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor(flac->fileName));
            REQUIRE(reader != nullptr);
            REQUIRE(reader->getFormatName() == "FLAC file");
            REQUIRE(reader->bitsPerSample == 24);
            reader.reset();

            auto bounced = audioFileToAudioBuffer(flac->fileName);
            auto source = audioFileToAudioBuffer(inputFile);
            REQUIRE(bounced.getNumSamples() == source.getNumSamples());

            // lossless: nothing beyond the 24-bit rounding of the float source
            const auto lsb = 1.0f / 8388608.0f;
            auto maxDiff = 0.0f;
            for (auto s = 0; s < bounced.getNumSamples(); s++)
                maxDiff = std::max(maxDiff, std::abs(bounced.getSample(0, s) - source.getSample(0, s)));
            INFO("largest deviation: " << maxDiff / lsb << " LSB");
            REQUIRE(maxDiff <= 2.0f * lsb);

            // and actually compressed: smaller than the 24-bit PCM it holds
            REQUIRE(flac->fileName.getSize() < (int64) bounced.getNumSamples() * 3);
        }
    }

    GIVEN("a FLAC export at 32 bits")
    {
        auto config = makeConfig("deep.flac", ExportFormat::flac);
        config->bitDepth = 32;

        THEN("it is refused, names format and depth, and writes nothing")
        {
            REQUIRE_FALSE(AudioExporter(*engine, config).bounce());
            REQUIRE(config->failure == ExportAudioConfig::Failure::unsupportedFormat);
            REQUIRE(config->error.contains("32"));
            REQUIRE(config->error.contains("FLAC"));
            REQUIRE(workDir.getNumberOfChildFiles(File::findFiles) == 0);
        }
    }

    GIVEN("a FLAC export of more channels than one FLAC file holds")
    {
        auto config = makeConfig("wide.flac", ExportFormat::flac);
        config->numChannels = 9;

        THEN("it is refused and writes nothing")
        {
            REQUIRE_FALSE(AudioExporter(*engine, config).bounce());
            REQUIRE(config->failure == ExportAudioConfig::Failure::unsupportedFormat);
            REQUIRE(config->error.contains("9"));
            REQUIRE(workDir.getNumberOfChildFiles(File::findFiles) == 0);
        }
    }

    GIVEN("a multi-mono FLAC export")
    {
        auto config = makeConfig("stems.flac", ExportFormat::flac);
        config->multiMono = true;

        REQUIRE(AudioExporter(*engine, config).bounce());

        THEN("only the FLAC mono files are left - no base file, no intermediate")
        {
            auto mono = AudioExporter::monoFileFor(config->fileName, 1, ExportFormat::flac);
            REQUIRE(mono.getFileName() == "stems-01.flac");
            REQUIRE(mono.existsAsFile());
            REQUIRE(audioFileToAudioBuffer(mono).getNumSamples() == 44100);
            REQUIRE(workDir.getNumberOfChildFiles(File::findFiles) == 1);
        }
    }

    workDir.deleteRecursively();
    if (inputFile.existsAsFile())
        inputFile.deleteFile();
}
