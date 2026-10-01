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

    GIVEN("a 24-bit FLAC and a 24-bit WAV bounce of the same project, one after the other")
    {
        auto flac = makeConfig("same.flac", ExportFormat::flac);
        auto wav = makeConfig("same.wav", ExportFormat::wav);
        REQUIRE(AudioExporter(*engine, flac).bounce());
        REQUIRE(AudioExporter(*engine, wav).bounce());

        THEN("the FLAC holds exactly the WAV's samples")
        {
            auto flacBuffer = audioFileToAudioBuffer(flac->fileName);
            auto wavBuffer = audioFileToAudioBuffer(wav->fileName);
            REQUIRE(flacBuffer.getNumSamples() == wavBuffer.getNumSamples());

            auto differing = 0;
            for (auto s = 0; s < flacBuffer.getNumSamples(); s++)
                if (flacBuffer.getSample(0, s) != wavBuffer.getSample(0, s))
                    differing++;
            REQUIRE(differing == 0);
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

// AIFF is lossless like WAV; Ogg Vorbis is lossy and takes a quality
// instead of a bit depth. Both share the refusals of the other formats.
SCENARIO("AIFF and Ogg Vorbis export", "[engine][export][aiff][ogg]")
{
    auto inputFile = createRampAudioFile(1.0);

    auto workDir = File::getSpecialLocation(File::tempDirectory).getChildFile("audionaut-aiff-ogg-export-tests");
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

    auto formatNameOf = [](const File& file) {
        AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor(file));
        return reader != nullptr ? reader->getFormatName() : String();
    };

    // the largest deviation from the source ramp over the given span
    auto maxDeviation = [&](const File& file, int from, int to) {
        auto bounced = audioFileToAudioBuffer(file);
        auto source = audioFileToAudioBuffer(inputFile);
        auto deviation = 0.0f;
        for (auto s = from; s < juce::jmin(to, bounced.getNumSamples(), source.getNumSamples()); s++)
            deviation = std::max(deviation, std::abs(bounced.getSample(0, s) - source.getSample(0, s)));
        return deviation;
    };

    GIVEN("a 24-bit AIFF bounce of a ramp")
    {
        auto aiff = makeConfig("bounce.aiff", ExportFormat::aiff);
        REQUIRE(AudioExporter(*engine, aiff).bounce());

        THEN("it is an AIFF file holding the ramp to within 24-bit quantisation")
        {
            REQUIRE(formatNameOf(aiff->fileName) == "AIFF file");
            REQUIRE(audioFileToAudioBuffer(aiff->fileName).getNumSamples() == 44100);
            REQUIRE(maxDeviation(aiff->fileName, 0, 44100) <= 2.0f / 8388608.0f);
        }
    }

    GIVEN("an AIFF export at 32 bits")
    {
        auto config = makeConfig("deep.aiff", ExportFormat::aiff);
        config->bitDepth = 32;

        THEN("it is refused and writes nothing")
        {
            REQUIRE_FALSE(AudioExporter(*engine, config).bounce());
            REQUIRE(config->failure == ExportAudioConfig::Failure::unsupportedFormat);
            REQUIRE(config->error.contains("an AIFF file"));
            REQUIRE(workDir.getNumberOfChildFiles(File::findFiles) == 0);
        }
    }

    GIVEN("an Ogg Vorbis bounce at the default quality")
    {
        auto ogg = makeConfig("bounce.ogg", ExportFormat::ogg);
        ogg->bitDepth = 12; // ignored: a lossy format has no bit depth
        REQUIRE(AudioExporter(*engine, ogg).bounce());

        THEN("it is an Ogg Vorbis file of the full length, close to the ramp")
        {
            REQUIRE(formatNameOf(ogg->fileName) == "Ogg-Vorbis file");
            REQUIRE(audioFileToAudioBuffer(ogg->fileName).getNumSamples() == 44100);

            // lossy, but a slow ramp survives well away from the file's edges
            auto deviation = maxDeviation(ogg->fileName, 4410, 44100 - 4410);
            INFO("largest deviation: " << deviation);
            REQUIRE(deviation < 0.02f);
        }
    }

    GIVEN("Ogg Vorbis bounces at the lowest and the highest quality")
    {
        auto low = makeConfig("low.ogg", ExportFormat::ogg);
        low->quality = 0;
        auto high = makeConfig("high.ogg", ExportFormat::ogg);
        high->quality = 10;
        REQUIRE(AudioExporter(*engine, low).bounce());
        REQUIRE(AudioExporter(*engine, high).bounce());

        THEN("the higher quality takes more bytes")
        {
            REQUIRE(low->fileName.getSize() < high->fileName.getSize());
        }
    }

    GIVEN("an Ogg Vorbis quality beyond the scale")
    {
        auto config = makeConfig("loud.ogg", ExportFormat::ogg);
        config->quality = 11;

        THEN("it is refused, names the range, and writes nothing")
        {
            REQUIRE_FALSE(AudioExporter(*engine, config).bounce());
            REQUIRE(config->failure == ExportAudioConfig::Failure::unsupportedFormat);
            REQUIRE(config->error.contains("0 to 10"));
            REQUIRE(workDir.getNumberOfChildFiles(File::findFiles) == 0);
        }
    }

    GIVEN("an Ogg Vorbis export of more channels than Vorbis lays out")
    {
        auto config = makeConfig("wide.ogg", ExportFormat::ogg);
        config->numChannels = 9;

        THEN("it is refused and writes nothing")
        {
            REQUIRE_FALSE(AudioExporter(*engine, config).bounce());
            REQUIRE(config->failure == ExportAudioConfig::Failure::unsupportedFormat);
            REQUIRE(config->error.contains("an Ogg Vorbis file cannot hold 9"));
            REQUIRE(workDir.getNumberOfChildFiles(File::findFiles) == 0);
        }
    }

    GIVEN("a multi-mono Ogg Vorbis export")
    {
        auto config = makeConfig("stems.ogg", ExportFormat::ogg);
        config->multiMono = true;
        REQUIRE(AudioExporter(*engine, config).bounce());

        THEN("only the Ogg mono files are left - no base file, no intermediate")
        {
            auto mono = AudioExporter::monoFileFor(config->fileName, 1, ExportFormat::ogg);
            REQUIRE(mono.getFileName() == "stems-01.ogg");
            REQUIRE(formatNameOf(mono) == "Ogg-Vorbis file");
            REQUIRE(workDir.getNumberOfChildFiles(File::findFiles) == 1);
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
