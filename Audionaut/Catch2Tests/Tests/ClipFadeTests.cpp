#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/ProjectFileStore.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/AudioSources/ClipTransportSource.h"
#include "Engine/Export/AudioExporter.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"

#include "TestUtils.h"

using namespace audium;
using namespace juce;

// With a DC +1.0 input every output sample equals the applied gain envelope,
// so the fade curve, NaNs and premature silence are all directly observable.

static void examineFadeOutEnvelope(const AudioBuffer<float>& buffer,
                                   int fadeStartSample,
                                   int totalSamples,
                                   int blockSize)
{
    // the scheduled fade start is block-quantised, allow one block of slack;
    // skip the resampler warm-up ripple at the very start
    const auto slack = blockSize;
    const auto warmUp = 256;

    // no NaN / Inf anywhere
    for (auto s = 0; s < totalSamples; s++) {
        auto val = buffer.getSample(0, s);
        INFO("sample " << s << " = " << val);
        REQUIRE(std::isfinite(val));
    }

    // before the fade starts the signal must be untouched (no dropout)
    for (auto s = warmUp; s < fadeStartSample - slack; s++) {
        auto val = buffer.getSample(0, s);
        INFO("pre-fade sample " << s << " = " << val);
        REQUIRE(val == Catch::Approx(1.f).margin(0.01f));
    }

    // inside the fade: monotonically non-increasing, within [0, 1]
    auto previous = 1.f;
    for (auto s = fadeStartSample + slack; s < totalSamples; s++) {
        auto val = buffer.getSample(0, s);
        INFO("fade sample " << s << " = " << val << " previous " << previous);
        REQUIRE(val >= 0.f);
        REQUIRE(val <= previous + 0.005f);
        previous = val;
    }

    // the fade is a ramp, not a mute: halfway in, the level must still be audible
    auto fadeLength = totalSamples - fadeStartSample;
    auto midpoint = buffer.getSample(0, fadeStartSample + fadeLength / 2);
    INFO("fade midpoint = " << midpoint);
    REQUIRE(midpoint > 0.5f);

    // and it must have (nearly) reached silence at the end - the fade must not
    // stop short and cut the clip while it is still audible
    auto last = buffer.getSample(0, totalSamples - 1);
    INFO("last sample = " << last);
    REQUIRE(last < 0.05f);
}

// The mirror of examineFadeOutEnvelope: silence up to silenceEndSample, a
// monotonically rising sqrt ramp to fadeEndSample, unity plateau after.
static void examineFadeInEnvelope(const AudioBuffer<float>& buffer,
                                  int silenceEndSample,
                                  int fadeEndSample,
                                  int totalSamples,
                                  int blockSize)
{
    const auto slack = blockSize;
    const auto warmUp = 256;

    // no NaN / Inf anywhere
    for (auto s = 0; s < totalSamples; s++) {
        auto val = buffer.getSample(0, s);
        INFO("sample " << s << " = " << val);
        REQUIRE(std::isfinite(val));
    }

    // before the ramp: silence
    if (silenceEndSample > warmUp + slack) {
        auto mag = buffer.getMagnitude(0, warmUp, silenceEndSample - warmUp - slack);
        INFO("head magnitude = " << mag);
        REQUIRE(mag == Catch::Approx(0.0).margin(0.01));
    }

    // inside the ramp: monotonically non-decreasing, within [0, 1]
    auto previous = 0.f;
    for (auto s = silenceEndSample + slack; s < fadeEndSample - slack; s++) {
        auto val = buffer.getSample(0, s);
        INFO("fade sample " << s << " = " << val << " previous " << previous);
        REQUIRE(val >= previous - 0.005f);
        REQUIRE(val <= 1.005f);
        previous = val;
    }

    // the ramp is a sqrt curve: halfway in, the level must already be audible
    auto fadeLength = fadeEndSample - silenceEndSample;
    auto midpoint = buffer.getSample(0, silenceEndSample + fadeLength / 2);
    INFO("fade midpoint = " << midpoint);
    REQUIRE(midpoint > 0.5f);

    // after the ramp: untouched plateau
    for (auto s = fadeEndSample + slack; s < totalSamples - slack; s++) {
        auto val = buffer.getSample(0, s);
        INFO("plateau sample " << s << " = " << val);
        REQUIRE(val == Catch::Approx(1.f).margin(0.01f));
    }
}

SCENARIO("fade out on the transport source", "[engine][dsp][fade]")
{
    auto sr = 44100.0;
    auto blockSize = 512;
    auto lengthSeconds = 2.0;
    auto fadeOutSeconds = 1.0;
    auto totalSamples = static_cast<int>(sr * lengthSeconds);

    GIVEN("a transport source over a 2 second DC signal with a 1 second fade out")
    {
        AudioBuffer<float> dcBuffer(1, totalSamples);
        for (auto s = 0; s < totalSamples; s++)
            dcBuffer.setSample(0, s, 1.f);

        MemoryAudioSource memorySource(dcBuffer, false);

        audium::ClipTransportSource transportSource;
        transportSource.setSource(&memorySource, 0, nullptr, 0.0, 1);
        transportSource.prepareToPlay(blockSize, sr);
        transportSource.resetClipGain();

        // as the scheduler does when a clip is scheduled
        transportSource.clearFadeIn();
        transportSource.setFadeOutRamp(fadeOutSeconds, lengthSeconds - fadeOutSeconds, true);
        transportSource.start();

        WHEN("rendering the full 2 seconds block by block")
        {
            AudioBuffer<float> output(1, totalSamples);
            output.clear();

            AudioBuffer<float> block(1, blockSize);
            auto rendered = 0;
            while (rendered < totalSamples) {
                auto numSamples = jmin(blockSize, totalSamples - rendered);
                AudioSourceChannelInfo info(&block, 0, numSamples);
                info.clearActiveBufferRegion();
                transportSource.getNextAudioBlock(info);
                output.copyFrom(0, rendered, block, 0, 0, numSamples);
                rendered += numSamples;
            }

            THEN("the first second is untouched and the second second fades to silence")
            {
                auto fadeStartSample = static_cast<int>(sr * (lengthSeconds - fadeOutSeconds));
                examineFadeOutEnvelope(output, fadeStartSample, totalSamples, blockSize);
            }
        }
    }
}

SCENARIO("fade out on a playlist item survives a bounce", "[engine][dsp][fade]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto sr = 44100.0;
    auto inputFile = generateDcOffsetAudioFile(2.0);

    auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
    bounceConfig->fileName = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/fade-out.wav"));
    bounceConfig->sampleRate = sr;
    bounceConfig->blockSize = 512;
    bounceConfig->numChannels = 1;

    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    GIVEN("a 2 second DC clip with a fade out over its second half")
    {
        store->open(inputFile, nullptr);

        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        auto item = track->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(item != nullptr);

        item->getDynamics().setFadeOut(0.5);
        REQUIRE(item->getDynamics().getFadeOut() == Catch::Approx(0.5));

        engine->getPlayListScheduler()->commitPlayListData();

        bounceConfig->lengthSeconds = engine->getPlayListScheduler()->getTotalLength(audium::seconds);
        REQUIRE(2.0 == Catch::Approx(bounceConfig->lengthSeconds));

        WHEN("bouncing the session")
        {
            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce();

            THEN("the first second is untouched and the second second fades to silence")
            {
                auto buffer = audioFileToAudioBuffer(bounceConfig->fileName);
                auto totalSamples = static_cast<int>(sr * 2.0);
                REQUIRE(buffer.getNumSamples() >= totalSamples);

                auto fadeStartSample = static_cast<int>(sr * 1.0);
                examineFadeOutEnvelope(buffer, fadeStartSample, totalSamples, bounceConfig->blockSize);
            }
        }

        WHEN("bouncing the session at 48 kHz (resampling the 44.1 kHz file)")
        {
            auto bounceSr = 48000.0;
            bounceConfig->sampleRate = bounceSr;

            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce();

            THEN("the first second is untouched and the second second fades to silence")
            {
                auto buffer = audioFileToAudioBuffer(bounceConfig->fileName);
                auto totalSamples = static_cast<int>(bounceSr * 2.0);
                REQUIRE(buffer.getNumSamples() >= totalSamples);

                auto fadeStartSample = static_cast<int>(bounceSr * 1.0);
                examineFadeOutEnvelope(buffer, fadeStartSample, totalSamples, bounceConfig->blockSize);
            }
        }
    }

    engine = nullptr;

    if (bounceConfig->fileName.existsAsFile())
        bounceConfig->fileName.deleteFile();

    if (inputFile.existsAsFile())
        inputFile.deleteFile();

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO("channel levels stay finite when a clip fades out", "[engine][dsp][fade][level]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto sr = 44100.0;
    auto inputFile = generateDcOffsetAudioFile(2.0);

    auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
    bounceConfig->fileName = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/fade-out-levels.wav"));
    bounceConfig->sampleRate = sr;
    bounceConfig->blockSize = 512;
    bounceConfig->numChannels = 2; // stereo: exercises panners, master gain and master level

    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    GIVEN("a 2 second DC clip with a fade out over its second half")
    {
        store->open(inputFile, nullptr);

        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        auto item = track->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(item != nullptr);

        item->getDynamics().setFadeOut(0.5);
        engine->getPlayListScheduler()->commitPlayListData();

        bounceConfig->lengthSeconds = engine->getPlayListScheduler()->getTotalLength(audium::seconds);

        WHEN("bouncing in stereo while polling the levels like the channel view does")
        {
            // collect problems instead of asserting inside the audio callback
            std::vector<std::string> badLevels;
            auto blockCounter = 0;

            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce([&](double) {
                for (auto c = 0; c < MAX_AUDIO_CHANNELS; ++c) {
                    auto lvl = engine->getAudioBusInterface()->getChannelLevel(c);
                    if (!std::isfinite(lvl))
                        badLevels.push_back("block " + std::to_string(blockCounter) +
                                            " channel " + std::to_string(c) +
                                            " level " + std::to_string(lvl));
                }
                for (auto m = 0; m < 2; ++m) {
                    auto lvl = engine->getAudioBusInterface()->getMasterLevel(m);
                    if (!std::isfinite(lvl))
                        badLevels.push_back("block " + std::to_string(blockCounter) +
                                            " master " + std::to_string(m) +
                                            " level " + std::to_string(lvl));
                }
                blockCounter++;
                return true;
            });

            THEN("every level is a number and the mix contains no NaN")
            {
                for (const auto& bad : badLevels) {
                    INFO(bad);
                }
                REQUIRE(badLevels.empty());

                // levels polled after playback (fade completed) must be finite too
                for (auto c = 0; c < MAX_AUDIO_CHANNELS; ++c) {
                    auto lvl = engine->getAudioBusInterface()->getChannelLevel(c);
                    INFO("channel " << c << " level " << lvl);
                    REQUIRE(std::isfinite(lvl));
                }

                auto buffer = audioFileToAudioBuffer(bounceConfig->fileName);
                for (auto c = 0; c < buffer.getNumChannels(); c++) {
                    for (auto s = 0; s < buffer.getNumSamples(); s++) {
                        auto val = buffer.getSample(c, s);
                        INFO("channel " << c << " sample " << s << " = " << val);
                        REQUIRE(std::isfinite(val));
                    }
                }
            }
        }
    }

    engine = nullptr;

    if (bounceConfig->fileName.existsAsFile())
        bounceConfig->fileName.deleteFile();

    if (inputFile.existsAsFile())
        inputFile.deleteFile();

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

// like generateDcOffsetAudioFile but with a caller-chosen name, so a session
// can hold two distinct DC files
static const File generateNamedDcOffsetAudioFile(const String& name, double lengthInSeconds)
{
    auto targetFile = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/") + name);
    TemporaryFile tempFile (targetFile);

    std::unique_ptr<OutputStream> stream (tempFile.getFile().createOutputStream());
    jassert(stream);
    if (stream != nullptr) {
        WavAudioFormat wav;
        auto opt = AudioFormatWriter::Options{}.withSampleRate (44100.0)
            .withNumChannels (1)
            .withBitsPerSample (32)
            .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
        auto writer = wav.createWriterFor (stream, opt);
        jassert(writer);
        if (writer != nullptr) {
            auto blockSize = static_cast<int>(44100.0 * lengthInSeconds);
            AudioBuffer<float> buffer(1, blockSize);
            for (auto s = 0; s < blockSize; s++) {
                buffer.setSample(0, s, 1.f);
            }
            writer->writeFromAudioSampleBuffer(buffer, 0, blockSize);
            writer.reset();
            tempFile.overwriteTargetFileWithTemporary();
            return tempFile.getTargetFile();
        }
    }
    return File();
}

SCENARIO("two overlapping clips, one fades out", "[engine][dsp][fade][level]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto sr = 44100.0;
    auto inputFileA = generateDcOffsetAudioFile(2.0);
    auto inputFileB = generateNamedDcOffsetAudioFile("dc-offset-b.wav", 4.0);

    auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
    bounceConfig->fileName = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/fade-out-overlap.wav"));
    bounceConfig->sampleRate = sr;
    bounceConfig->blockSize = 512;
    bounceConfig->numChannels = 2;

    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    GIVEN("clip A (2 s, fade out) and clip B (4 s) playing simultaneously on two tracks")
    {
        store->open(inputFileA, nullptr);

        auto ok = engine->getAudioTrackContainer()->addAudioFiles({ inputFileB.getFullPathName() },
                                                                  0.0, nullptr, false);
        REQUIRE(ok);
        REQUIRE(engine->getAudioTrackContainer()->getAudioTracks().size() == 2);

        auto trackA = engine->getAudioTrackContainer()->getAudioTrack(0);
        auto itemA = trackA->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(itemA != nullptr);

        auto trackB = engine->getAudioTrackContainer()->getAudioTrack(1);
        auto itemB = trackB->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(itemB != nullptr);
        REQUIRE(itemB->getRegionData(audium::seconds).getLength() == Catch::Approx(4.0));

        // fade out over the second half of clip A only
        itemA->getDynamics().setFadeOut(0.5);

        engine->getPlayListScheduler()->commitPlayListData();

        auto bounceSr = GENERATE(44100.0, 48000.0);

        WHEN("bouncing past clip A's end at " + std::to_string(static_cast<int>(bounceSr)) + " Hz while polling the levels")
        {
            std::vector<std::string> badLevels;
            auto blockCounter = 0;

            bounceConfig->sampleRate = bounceSr;
            bounceConfig->lengthSeconds = 5.0;

            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce([&](double) {
                for (auto c = 0; c < MAX_AUDIO_CHANNELS; ++c) {
                    auto lvl = engine->getAudioBusInterface()->getChannelLevel(c);
                    if (!std::isfinite(lvl))
                        badLevels.push_back("block " + std::to_string(blockCounter) +
                                            " channel " + std::to_string(c) +
                                            " level " + std::to_string(lvl));
                }
                blockCounter++;
                return true;
            });

            THEN("levels stay numbers and clip B keeps playing after A's fade out")
            {
                for (const auto& bad : badLevels) {
                    std::cout << "bad level: " << bad << std::endl;
                }
                REQUIRE(badLevels.empty());

                auto buffer = audioFileToAudioBuffer(bounceConfig->fileName);
                auto totalSamples = static_cast<int>(bounceSr * 5.0);
                REQUIRE(buffer.getNumSamples() >= totalSamples);

                // no NaN / Inf anywhere in the mix
                for (auto c = 0; c < buffer.getNumChannels(); c++) {
                    for (auto s = 0; s < totalSamples; s++) {
                        auto val = buffer.getSample(c, s);
                        INFO("channel " << c << " sample " << s << " = " << val);
                        REQUIRE(std::isfinite(val));
                    }
                }

                // shortly after A's fade out ends, B must still be audible
                auto mag = buffer.getMagnitude(0, static_cast<int>(bounceSr * 2.1),
                                               static_cast<int>(bounceSr * 1.0));
                INFO("clip B magnitude after A ended = " << mag);
                REQUIRE(std::isfinite(mag));
                REQUIRE(mag > 0.25);

                // and B must end, leaving silence at second 4.5
                mag = buffer.getMagnitude(0, static_cast<int>(bounceSr * 4.5),
                                          static_cast<int>(bounceSr * 0.4));
                REQUIRE(mag == Catch::Approx(0.0).margin(0.000001));
            }
        }
    }

    engine = nullptr;

    if (bounceConfig->fileName.existsAsFile())
        bounceConfig->fileName.deleteFile();

    if (inputFileA.existsAsFile())
        inputFileA.deleteFile();

    if (inputFileB.existsAsFile())
        inputFileB.deleteFile();

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO("the mix continues after a faded clip ends", "[engine][dsp][fade][level]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto sr = 44100.0;
    auto inputFile = generateDcOffsetAudioFile(2.0);

    auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
    bounceConfig->fileName = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/fade-out-mix.wav"));
    bounceConfig->sampleRate = sr;
    bounceConfig->blockSize = 512;
    bounceConfig->numChannels = 2;

    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    GIVEN("a faded clip followed by a second clip sharing the same region")
    {
        store->open(inputFile, nullptr);

        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        auto item0 = track->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(item0 != nullptr);

        auto region = item0->getRegion();
        auto item1 = track->getPlayListContainer()->createPlayListItemAtPositionUI(region, 2.5, audium::seconds);
        REQUIRE(item1 != nullptr);

        // fade out over the second half of the first clip only
        item0->getDynamics().setFadeOut(0.5);

        engine->getPlayListScheduler()->commitPlayListData();

        bounceConfig->lengthSeconds = 5.0;

        WHEN("bouncing past the faded clip while polling the levels")
        {
            std::vector<std::string> badLevels;
            auto blockCounter = 0;

            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce([&](double) {
                for (auto c = 0; c < MAX_AUDIO_CHANNELS; ++c) {
                    auto lvl = engine->getAudioBusInterface()->getChannelLevel(c);
                    if (!std::isfinite(lvl))
                        badLevels.push_back("block " + std::to_string(blockCounter) +
                                            " channel " + std::to_string(c) +
                                            " level " + std::to_string(lvl));
                }
                blockCounter++;
                return true;
            });

            THEN("levels stay numbers and the second clip is still audible")
            {
                for (const auto& bad : badLevels) {
                    INFO(bad);
                }
                REQUIRE(badLevels.empty());

                auto buffer = audioFileToAudioBuffer(bounceConfig->fileName);
                auto totalSamples = static_cast<int>(sr * 5.0);
                REQUIRE(buffer.getNumSamples() >= totalSamples);

                // no NaN / Inf anywhere in the mix
                for (auto c = 0; c < buffer.getNumChannels(); c++) {
                    for (auto s = 0; s < totalSamples; s++) {
                        auto val = buffer.getSample(c, s);
                        INFO("channel " << c << " sample " << s << " = " << val);
                        REQUIRE(std::isfinite(val));
                    }
                }

                // silence between the clips
                auto mag = buffer.getMagnitude(0, static_cast<int>(sr * 2.1),
                                               static_cast<int>(sr * 0.3));
                REQUIRE(mag == Catch::Approx(0.0).margin(0.000001));

                // the second clip must play at full level - the mix must not
                // stay broken after the first clip's fade out
                mag = buffer.getMagnitude(0, static_cast<int>(sr * 2.75),
                                          static_cast<int>(sr * 0.5));
                INFO("second clip magnitude = " << mag);
                REQUIRE(std::isfinite(mag));
                REQUIRE(mag > 0.5);
            }
        }
    }

    engine = nullptr;

    if (bounceConfig->fileName.existsAsFile())
        bounceConfig->fileName.deleteFile();

    if (inputFile.existsAsFile())
        inputFile.deleteFile();

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO("fades reset when restoring an item state without fades", "[engine][fade]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto inputFile = generateDcOffsetAudioFile(2.0);
    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    GIVEN("a playlist item with fades")
    {
        store->open(inputFile, nullptr);

        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        auto item = track->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(item != nullptr);

        item->getDynamics().setFadeIn(0.25);
        item->getDynamics().setFadeOut(0.5);
        item->getDynamics().setFadeInStart(-0.1); // outside the clip
        item->getDynamics().setFadeOutEnd(0.2);

        WHEN("reading a state without fade keys into the same item (undo)")
        {
            json state;
            item->getDynamics().readFromJson(state);

            THEN("all fade values are reset")
            {
                REQUIRE(item->getDynamics().getFadeIn() == Catch::Approx(0.0));
                REQUIRE(item->getDynamics().getFadeOut() == Catch::Approx(0.0));
                REQUIRE(item->getDynamics().getFadeInStart() == Catch::Approx(0.0));
                REQUIRE(item->getDynamics().getFadeOutEnd() == Catch::Approx(0.0));
            }
        }
    }

    engine = nullptr;

    if (inputFile.existsAsFile())
        inputFile.deleteFile();

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO("fade ramp offsets push their partner values", "[engine][fade]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto inputFile = generateDcOffsetAudioFile(2.0);
    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    GIVEN("a playlist item with a fade in and a fade out")
    {
        store->open(inputFile, nullptr);

        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        auto item = track->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(item != nullptr);

        auto& dynamics = item->getDynamics();
        dynamics.setFadeIn(0.5);
        dynamics.setFadeOut(0.4);

        WHEN("the fade in start stays below the fade in end")
        {
            auto pushed = dynamics.setFadeInStart(0.25);

            THEN("nothing else moves")
            {
                REQUIRE_FALSE(pushed);
                REQUIRE(dynamics.getFadeInStart() == Catch::Approx(0.25));
                REQUIRE(dynamics.getFadeIn() == Catch::Approx(0.5));
                REQUIRE(dynamics.getFadeOut() == Catch::Approx(0.4));
            }
        }

        WHEN("the fade in start is dragged past the fade in end")
        {
            auto pushed = dynamics.setFadeInStart(0.8);

            THEN("the fade in end is pushed along and the fade out pushed back")
            {
                REQUIRE(pushed);
                REQUIRE(dynamics.getFadeInStart() == Catch::Approx(0.8));
                REQUIRE(dynamics.getFadeIn() == Catch::Approx(0.8));
                REQUIRE(dynamics.getFadeOut() == Catch::Approx(0.2));
            }
        }

        WHEN("the push cascades into the fade out end")
        {
            dynamics.setFadeOutEnd(0.3);
            auto pushed = dynamics.setFadeInStart(0.9);

            THEN("the whole fade out pair is pulled down")
            {
                REQUIRE(pushed);
                REQUIRE(dynamics.getFadeIn() == Catch::Approx(0.9));
                REQUIRE(dynamics.getFadeOut() == Catch::Approx(0.1));
                REQUIRE(dynamics.getFadeOutEnd() == Catch::Approx(0.1));
            }
        }

        WHEN("the fade in end is dragged below the fade in start")
        {
            dynamics.setFadeInStart(0.25);
            auto pushed = dynamics.setFadeIn(0.1);

            THEN("the fade in start is pulled down")
            {
                REQUIRE(pushed);
                REQUIRE(dynamics.getFadeIn() == Catch::Approx(0.1));
                REQUIRE(dynamics.getFadeInStart() == Catch::Approx(0.1));
            }
        }

        WHEN("the fade out end is dragged past the fade out start")
        {
            auto pushed = dynamics.setFadeOutEnd(0.6);

            THEN("the fade out start is pushed along and the fade in pushed back")
            {
                REQUIRE(pushed);
                REQUIRE(dynamics.getFadeOutEnd() == Catch::Approx(0.6));
                REQUIRE(dynamics.getFadeOut() == Catch::Approx(0.6));
                REQUIRE(dynamics.getFadeIn() == Catch::Approx(0.4));
            }
        }

        WHEN("the fade out start is dragged below the fade out end")
        {
            dynamics.setFadeOutEnd(0.2);
            auto pushed = dynamics.setFadeOut(0.1);

            THEN("the fade out end is pulled down")
            {
                REQUIRE(pushed);
                REQUIRE(dynamics.getFadeOut() == Catch::Approx(0.1));
                REQUIRE(dynamics.getFadeOutEnd() == Catch::Approx(0.1));
            }
        }

        WHEN("the fade in start is dragged outside the clip")
        {
            auto pushed = dynamics.setFadeInStart(-0.25);

            THEN("the negative offset is stored and nothing else moves")
            {
                REQUIRE_FALSE(pushed);
                REQUIRE(dynamics.getFadeInStart() == Catch::Approx(-0.25));
                REQUIRE(dynamics.getFadeIn() == Catch::Approx(0.5));
                REQUIRE(dynamics.getFadeOut() == Catch::Approx(0.4));
            }
        }

        WHEN("the fade in end is dragged after the start went outside the clip")
        {
            dynamics.setFadeInStart(-0.25);
            auto pushed = dynamics.setFadeIn(0.1);

            THEN("the extension survives - no pull towards the clip")
            {
                REQUIRE_FALSE(pushed);
                REQUIRE(dynamics.getFadeIn() == Catch::Approx(0.1));
                REQUIRE(dynamics.getFadeInStart() == Catch::Approx(-0.25));
            }
        }

        WHEN("a cascade drives the fade in to zero while its start is outside")
        {
            dynamics.setFadeInStart(-0.2);
            auto pushed = dynamics.setFadeOut(1.0);

            THEN("the extension survives the cascade")
            {
                REQUIRE(pushed);
                REQUIRE(dynamics.getFadeIn() == Catch::Approx(0.0));
                REQUIRE(dynamics.getFadeInStart() == Catch::Approx(-0.2));
            }
        }

        WHEN("the fade out end is dragged outside the clip")
        {
            auto pushed = dynamics.setFadeOutEnd(-0.3);
            dynamics.setFadeOut(0.1);

            THEN("the negative offset is stored and survives fade edits")
            {
                REQUIRE_FALSE(pushed);
                REQUIRE(dynamics.getFadeOutEnd() == Catch::Approx(-0.3));
                REQUIRE(dynamics.getFadeOut() == Catch::Approx(0.1));
            }
        }

        WHEN("an offset beyond the far clip edge is requested")
        {
            auto pushed = dynamics.setFadeInStart(1.5);

            THEN("it clamps to the clip length and pushes the fades")
            {
                REQUIRE(pushed);
                REQUIRE(dynamics.getFadeInStart() == Catch::Approx(1.0));
                REQUIRE(dynamics.getFadeIn() == Catch::Approx(1.0));
                REQUIRE(dynamics.getFadeOut() == Catch::Approx(0.0));
            }
        }
    }

    engine = nullptr;

    if (inputFile.existsAsFile())
        inputFile.deleteFile();

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO("fade ramp offsets serialize with the item", "[engine][fade]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto inputFile = generateDcOffsetAudioFile(2.0);
    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    GIVEN("a playlist item")
    {
        store->open(inputFile, nullptr);

        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        auto item = track->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(item != nullptr);

        auto& dynamics = item->getDynamics();

        WHEN("nothing but the plain fades is set")
        {
            dynamics.setFadeIn(0.25);
            dynamics.setFadeOut(0.5);

            json state;
            dynamics.writeToJson(state);

            THEN("no ramp offset keys are written")
            {
                REQUIRE_FALSE(state.contains("fade_in_start_clocks"));
                REQUIRE_FALSE(state.contains("fade_out_end_clocks"));
            }
        }

        WHEN("all four fade values round-trip through json")
        {
            dynamics.setFadeIn(0.5);
            dynamics.setFadeOut(0.4);
            dynamics.setFadeInStart(0.25);
            dynamics.setFadeOutEnd(0.3);

            json state;
            dynamics.writeToJson(state);

            // overwrite before restoring, so the read is observable
            dynamics.setFadeInStart(0.0);
            dynamics.setFadeOutEnd(0.0);
            dynamics.setFadeIn(0.75);
            dynamics.setFadeOut(0.1);

            dynamics.readFromJson(state);

            THEN("all four values are restored")
            {
                REQUIRE(dynamics.getFadeIn() == Catch::Approx(0.5));
                REQUIRE(dynamics.getFadeOut() == Catch::Approx(0.4));
                REQUIRE(dynamics.getFadeInStart() == Catch::Approx(0.25));
                REQUIRE(dynamics.getFadeOutEnd() == Catch::Approx(0.3));
            }
        }

        WHEN("the curve exponents round-trip through json and reset with it")
        {
            dynamics.setFadeInCurve(1.0);
            dynamics.setFadeOutCurve(2.0);

            json state;
            dynamics.writeToJson(state);
            REQUIRE(state.contains("fade_in_curve"));
            REQUIRE(state.contains("fade_out_curve"));

            dynamics.setFadeInCurve(0.5);
            dynamics.setFadeOutCurve(0.5);
            dynamics.readFromJson(state);

            THEN("the curves are restored")
            {
                REQUIRE(dynamics.getFadeInCurve() == Catch::Approx(1.0));
                REQUIRE(dynamics.getFadeOutCurve() == Catch::Approx(2.0));
            }

            json empty;
            dynamics.readFromJson(empty);

            THEN("reading a state without curve keys resets to the default")
            {
                REQUIRE(dynamics.getFadeInCurve() == Catch::Approx(ClipDynamics::defaultFadeCurve));
                REQUIRE(dynamics.getFadeOutCurve() == Catch::Approx(ClipDynamics::defaultFadeCurve));
            }
        }

        WHEN("a default curve writes no key and setters clamp the exponent")
        {
            json state;
            dynamics.writeToJson(state);
            REQUIRE_FALSE(state.contains("fade_in_curve"));
            REQUIRE_FALSE(state.contains("fade_out_curve"));

            dynamics.setFadeInCurve(100.0);
            dynamics.setFadeOutCurve(0.0);

            THEN("the exponents stay within the legal range")
            {
                REQUIRE(dynamics.getFadeInCurve() == Catch::Approx(ClipDynamics::maxFadeCurve));
                REQUIRE(dynamics.getFadeOutCurve() == Catch::Approx(ClipDynamics::minFadeCurve));
            }
        }

        WHEN("negative ramp offsets round-trip through json")
        {
            dynamics.setFadeIn(0.5);
            dynamics.setFadeOut(0.4);
            dynamics.setFadeInStart(-0.25);
            dynamics.setFadeOutEnd(-0.3);

            json state;
            dynamics.writeToJson(state);

            // overwrite before restoring, so the read is observable
            dynamics.setFadeInStart(0.1);
            dynamics.setFadeOutEnd(0.2);

            dynamics.readFromJson(state);

            THEN("the outside-the-clip offsets are restored")
            {
                REQUIRE(dynamics.getFadeInStart() == Catch::Approx(-0.25));
                REQUIRE(dynamics.getFadeOutEnd() == Catch::Approx(-0.3));
            }
        }
    }

    engine = nullptr;

    if (inputFile.existsAsFile())
        inputFile.deleteFile();

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO("fade out applied while the clip is playing", "[engine][dsp][fade]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto sr = 44100.0;
    auto inputFile = generateDcOffsetAudioFile(2.0);

    auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
    bounceConfig->fileName = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/fade-out-live.wav"));
    bounceConfig->sampleRate = sr;
    bounceConfig->blockSize = 512;
    bounceConfig->numChannels = 1;

    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    GIVEN("a playing 2 second DC clip without fades")
    {
        store->open(inputFile, nullptr);

        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        auto item = track->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(item != nullptr);

        engine->getPlayListScheduler()->commitPlayListData();

        bounceConfig->lengthSeconds = engine->getPlayListScheduler()->getTotalLength(audium::seconds);
        REQUIRE(2.0 == Catch::Approx(bounceConfig->lengthSeconds));

        WHEN("a fade out over the second half is applied at second 0.5 during playback")
        {
            auto fadeApplied = false;
            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce([&](double progress) {
                if (!fadeApplied && progress >= 0.25) {
                    fadeApplied = true;
                    item->getDynamics().setFadeOut(0.5);
                    engine->getPlayListScheduler()->commitPlayListData();
                }
                return true;
            });
            REQUIRE(fadeApplied);

            THEN("the audio before the fade keeps playing and the fade runs over the second second")
            {
                auto buffer = audioFileToAudioBuffer(bounceConfig->fileName);
                auto totalSamples = static_cast<int>(sr * 2.0);
                REQUIRE(buffer.getNumSamples() >= totalSamples);

                // no NaN / Inf anywhere
                for (auto s = 0; s < totalSamples; s++) {
                    auto val = buffer.getSample(0, s);
                    INFO("sample " << s << " = " << val);
                    REQUIRE(std::isfinite(val));
                }

                auto slack = bounceConfig->blockSize * 2;

                // between applying the fade (0.5 s) and the fade start (1.0 s)
                // the audio must keep playing at full level - no dropout
                for (auto s = static_cast<int>(sr * 0.5) + slack; s < static_cast<int>(sr * 1.0) - slack; s++) {
                    auto val = buffer.getSample(0, s);
                    INFO("pre-fade sample " << s << " = " << val);
                    REQUIRE(val == Catch::Approx(1.f).margin(0.01f));
                }

                // fade midpoint still audible
                auto midpoint = buffer.getSample(0, static_cast<int>(sr * 1.5));
                INFO("fade midpoint = " << midpoint);
                REQUIRE(midpoint > 0.5f);

                // end (nearly) silent - the fade must not stop short and cut
                // the clip while it is still audible
                auto last = buffer.getSample(0, totalSamples - 1);
                INFO("last sample = " << last);
                REQUIRE(last < 0.05f);
            }
        }
    }

    engine = nullptr;

    if (bounceConfig->fileName.existsAsFile())
        bounceConfig->fileName.deleteFile();

    if (inputFile.existsAsFile())
        inputFile.deleteFile();

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO("fade out on a clip that does not start at zero", "[engine][dsp][fade]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto sr = 44100.0;
    auto inputFile = generateDcOffsetAudioFile(2.0);

    auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
    bounceConfig->fileName = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/fade-out-offset.wav"));
    bounceConfig->sampleRate = sr;
    bounceConfig->blockSize = 512;
    bounceConfig->numChannels = 1;

    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    GIVEN("a 2 second DC clip placed at second 1 with a fade out over its second half")
    {
        store->open(inputFile, nullptr);

        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        auto item = track->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(item != nullptr);

        item->setAbsolutePosition(1.0, audium::seconds);
        REQUIRE(item->getAbsolutePosition(audium::seconds) == Catch::Approx(1.0));

        item->getDynamics().setFadeOut(0.5);

        WHEN("bouncing the session with a fade out only")
        {
            engine->getPlayListScheduler()->commitPlayListData();

            bounceConfig->lengthSeconds = engine->getPlayListScheduler()->getTotalLength(audium::seconds);
            REQUIRE(3.0 == Catch::Approx(bounceConfig->lengthSeconds));

            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce();

            THEN("second 1 is silent, second 2 untouched, second 3 fades to silence")
            {
                auto buffer = audioFileToAudioBuffer(bounceConfig->fileName);
                auto totalSamples = static_cast<int>(sr * 3.0);
                REQUIRE(buffer.getNumSamples() >= totalSamples);

                auto slack = bounceConfig->blockSize;

                // leading silence before the clip
                auto mag = buffer.getMagnitude(0, static_cast<int>(sr * 1.0) - slack);
                REQUIRE(mag == Catch::Approx(0.0));

                // the clip audio with the fade over its second half
                AudioBuffer<float> clipAudio(1, static_cast<int>(sr * 2.0));
                clipAudio.copyFrom(0, 0, buffer, 0, static_cast<int>(sr * 1.0), clipAudio.getNumSamples());

                auto fadeStartSample = static_cast<int>(sr * 1.0);
                examineFadeOutEnvelope(clipAudio, fadeStartSample, clipAudio.getNumSamples(), bounceConfig->blockSize);
            }
        }

        WHEN("bouncing the session with fade in and fade out")
        {
            item->getDynamics().setFadeIn(0.25);
            engine->getPlayListScheduler()->commitPlayListData();

            bounceConfig->lengthSeconds = engine->getPlayListScheduler()->getTotalLength(audium::seconds);

            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce();

            THEN("the plateau between the fades is untouched and the end fades to silence")
            {
                auto buffer = audioFileToAudioBuffer(bounceConfig->fileName);
                auto totalSamples = static_cast<int>(sr * 3.0);
                REQUIRE(buffer.getNumSamples() >= totalSamples);

                auto slack = bounceConfig->blockSize;

                // no NaN / Inf anywhere
                for (auto s = 0; s < totalSamples; s++) {
                    auto val = buffer.getSample(0, s);
                    INFO("sample " << s << " = " << val);
                    REQUIRE(std::isfinite(val));
                }

                // fade in: 0.5 s ramp from second 1, rising
                auto early = buffer.getSample(0, static_cast<int>(sr * 1.1));
                auto late = buffer.getSample(0, static_cast<int>(sr * 1.4));
                INFO("fade in early " << early << " late " << late);
                REQUIRE(late > early);

                // plateau between the fades (1.5 s .. 2.0 s) must be untouched (no dropout)
                for (auto s = static_cast<int>(sr * 1.5) + slack; s < static_cast<int>(sr * 2.0) - slack; s++) {
                    auto val = buffer.getSample(0, s);
                    INFO("plateau sample " << s << " = " << val);
                    REQUIRE(val == Catch::Approx(1.f).margin(0.000001f));
                }

                // fade out midpoint still audible, end (nearly) silent
                auto midpoint = buffer.getSample(0, static_cast<int>(sr * 2.5));
                INFO("fade out midpoint = " << midpoint);
                REQUIRE(midpoint > 0.5f);

                auto last = buffer.getSample(0, totalSamples - 1);
                INFO("last sample = " << last);
                REQUIRE(last < 0.1f);
            }
        }
    }

    engine = nullptr;

    if (bounceConfig->fileName.existsAsFile())
        bounceConfig->fileName.deleteFile();

    if (inputFile.existsAsFile())
        inputFile.deleteFile();

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO("fade ramps with offsets on the transport source", "[engine][dsp][fade]")
{
    auto sr = 44100.0;
    auto blockSize = 512;
    auto lengthSeconds = 2.0;
    auto totalSamples = static_cast<int>(sr * lengthSeconds);

    GIVEN("a transport source over a 2 second DC signal")
    {
        AudioBuffer<float> dcBuffer(1, totalSamples);
        for (auto s = 0; s < totalSamples; s++)
            dcBuffer.setSample(0, s, 1.f);

        MemoryAudioSource memorySource(dcBuffer, false);

        audium::ClipTransportSource transportSource;
        transportSource.setSource(&memorySource, 0, nullptr, 0.0, 1);
        transportSource.prepareToPlay(blockSize, sr);
        transportSource.resetClipGain();

        auto render = [&]() {
            AudioBuffer<float> output(1, totalSamples);
            output.clear();

            AudioBuffer<float> block(1, blockSize);
            auto rendered = 0;
            while (rendered < totalSamples) {
                auto numSamples = jmin(blockSize, totalSamples - rendered);
                AudioSourceChannelInfo info(&block, 0, numSamples);
                info.clearActiveBufferRegion();
                transportSource.getNextAudioBlock(info);
                output.copyFrom(0, rendered, block, 0, 0, numSamples);
                rendered += numSamples;
            }
            return output;
        };

        WHEN("a fade-in ramp starts half a second in")
        {
            transportSource.setFadeInRamp(0.5, 0.5, true);
            transportSource.clearFadeOut();
            transportSource.start();

            auto output = render();

            THEN("the head is silent, then the ramp rises to unity")
            {
                examineFadeInEnvelope(output,
                                      static_cast<int>(sr * 0.5),
                                      static_cast<int>(sr * 1.0),
                                      totalSamples, blockSize);
            }
        }

        WHEN("a fade-in ramp is armed a quarter in (pre-elapsed)")
        {
            transportSource.setFadeInRamp(1.0, -0.25, true);
            transportSource.clearFadeOut();
            transportSource.start();

            auto output = render();

            THEN("the output starts mid-ramp and reaches unity early")
            {
                auto first = output.getSample(0, 16);
                INFO("first sample " << first);
                REQUIRE(first == Catch::Approx(std::sqrt(0.25)).margin(0.05));

                auto atRampEnd = output.getSample(0, static_cast<int>(sr * 0.75) + blockSize);
                REQUIRE(atRampEnd == Catch::Approx(1.0).margin(0.01));
            }
        }

        WHEN("a scheduled fade-out is reconfigured to a pre-elapsed ramp")
        {
            // regression: the skip counter of the first schedule must not
            // survive the reconfiguration
            transportSource.setFadeOutRamp(0.5, 1.0, true);
            transportSource.setFadeOutRamp(1.0, -0.5, true);
            transportSource.clearFadeIn();
            transportSource.start();

            auto output = render();

            THEN("the output starts mid-ramp and falls - no unity hold")
            {
                auto first = output.getSample(0, 16);
                INFO("first sample " << first);
                REQUIRE(first == Catch::Approx(std::sqrt(0.5)).margin(0.05));

                auto later = output.getSample(0, static_cast<int>(sr * 0.25));
                INFO("later sample " << later);
                REQUIRE(later < first);
            }
        }

        WHEN("a fade-out ramp ends early")
        {
            transportSource.clearFadeIn();
            transportSource.setFadeOutRamp(0.5, 0.5, true);
            transportSource.start();

            auto output = render();

            THEN("the ramp completes at second 1 and holds silence to the end")
            {
                examineFadeOutEnvelope(output,
                                       static_cast<int>(sr * 0.5),
                                       static_cast<int>(sr * 1.0),
                                       blockSize);

                auto tail = output.getMagnitude(0, static_cast<int>(sr * 1.0) + blockSize,
                                                totalSamples - static_cast<int>(sr * 1.0) - blockSize);
                INFO("tail magnitude " << tail);
                REQUIRE(tail == Catch::Approx(0.0).margin(0.01));
            }
        }
    }
}

SCENARIO("fade extensions extend the audible clip", "[engine][dsp][fade]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto sr = 44100.0;
    auto inputFile = generateDcOffsetAudioFile(2.0);

    auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
    bounceConfig->fileName = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/fade-extension.wav"));
    bounceConfig->sampleRate = sr;
    bounceConfig->blockSize = 512;
    bounceConfig->numChannels = 1;

    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    GIVEN("a clip trimmed to the middle second of a 2 second DC file, placed at second 1")
    {
        store->open(inputFile, nullptr);

        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        auto item = track->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(item != nullptr);

        // region window [0.5, 1.5] of the source file, timeline [1.5, 2.5]
        item->setAbsolutePosition(1.0, audium::seconds);
        item->setAbsoluteStartPosition(1.5, audium::seconds);
        item->setLength(1.0, audium::seconds);
        REQUIRE(item->getRegionData(audium::seconds).getStart() == Catch::Approx(0.5));
        REQUIRE(item->getRegionData(audium::seconds).getLength() == Catch::Approx(1.0));
        REQUIRE(item->getAbsolutePosition(audium::seconds) == Catch::Approx(1.5));

        WHEN("a negative fade-in start pulls in material before the clip")
        {
            item->getDynamics().setFadeIn(0.25);
            item->getDynamics().setFadeInStart(-0.25);
            engine->getPlayListScheduler()->commitPlayListData();

            bounceConfig->lengthSeconds = engine->getPlayListScheduler()->getTotalLength(audium::seconds);

            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce();

            THEN("the ramp rises across the clip boundary from second 1.25")
            {
                auto buffer = audioFileToAudioBuffer(bounceConfig->fileName);
                auto totalSamples = static_cast<int>(sr * 2.5);
                REQUIRE(buffer.getNumSamples() >= totalSamples);

                // audible start 1.25 (clip start 1.5 minus the 0.25 s
                // extension), ramp end at 1.75 (fade-in 0.25 into the clip)
                examineFadeInEnvelope(buffer,
                                      static_cast<int>(sr * 1.25),
                                      static_cast<int>(sr * 1.75),
                                      totalSamples, bounceConfig->blockSize);
            }
        }

        WHEN("a negative fade-out end lets the clip ring out past its end")
        {
            item->getDynamics().setFadeOut(0.25);
            item->getDynamics().setFadeOutEnd(-0.25);
            engine->getPlayListScheduler()->commitPlayListData();

            bounceConfig->lengthSeconds = engine->getPlayListScheduler()->getTotalLength(audium::seconds);

            THEN("the total length grew over the tail extension")
            {
                REQUIRE(bounceConfig->lengthSeconds == Catch::Approx(2.75));
            }

            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce();

            THEN("the ramp falls across the clip end and reaches silence at 2.75")
            {
                auto buffer = audioFileToAudioBuffer(bounceConfig->fileName);
                auto totalSamples = static_cast<int>(sr * 2.75);
                REQUIRE(buffer.getNumSamples() >= totalSamples);

                // clip body from second 1.5 with the fade over [2.25, 2.75]
                AudioBuffer<float> clipAudio(1, static_cast<int>(sr * 1.25));
                clipAudio.copyFrom(0, 0, buffer, 0, static_cast<int>(sr * 1.5), clipAudio.getNumSamples());

                examineFadeOutEnvelope(clipAudio,
                                       static_cast<int>(sr * 0.75),
                                       clipAudio.getNumSamples(),
                                       bounceConfig->blockSize);
            }
        }

        WHEN("a positive fade-in start leaves a silent head inside the clip")
        {
            item->getDynamics().setFadeIn(0.5);
            item->getDynamics().setFadeInStart(0.25);
            engine->getPlayListScheduler()->commitPlayListData();

            bounceConfig->lengthSeconds = engine->getPlayListScheduler()->getTotalLength(audium::seconds);

            THEN("the total length is unchanged")
            {
                REQUIRE(bounceConfig->lengthSeconds == Catch::Approx(2.5));
            }

            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce();

            THEN("the clip is silent until 1.75, ramping to unity at 2.0")
            {
                auto buffer = audioFileToAudioBuffer(bounceConfig->fileName);
                auto totalSamples = static_cast<int>(sr * 2.5);
                REQUIRE(buffer.getNumSamples() >= totalSamples);

                // silent head [1.5, 1.75] inside the clip, ramp [1.75, 2.0],
                // plateau to the clip end at 2.5
                examineFadeInEnvelope(buffer,
                                      static_cast<int>(sr * 1.75),
                                      static_cast<int>(sr * 2.0),
                                      totalSamples, bounceConfig->blockSize);
            }
        }

        WHEN("a linear fade-out curve renders a straight ramp")
        {
            item->getDynamics().setFadeOut(0.5);
            item->getDynamics().setFadeOutCurve(1.0);
            engine->getPlayListScheduler()->commitPlayListData();

            bounceConfig->lengthSeconds = engine->getPlayListScheduler()->getTotalLength(audium::seconds);

            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce();

            THEN("the ramp midpoint is 0.5 - not the equal-power 0.707")
            {
                auto buffer = audioFileToAudioBuffer(bounceConfig->fileName);
                REQUIRE(buffer.getNumSamples() >= static_cast<int>(sr * 2.5));

                // fade over timeline [2.0, 2.5]; linear -> midpoint == 0.5
                auto midpoint = buffer.getSample(0, static_cast<int>(sr * 2.25));
                INFO("midpoint " << midpoint);
                REQUIRE(midpoint == Catch::Approx(0.5).margin(0.03));

                auto last = buffer.getSample(0, static_cast<int>(sr * 2.5) - 1);
                REQUIRE(last < 0.05f);
            }
        }

        WHEN("the item is bounced on its own with a tail extension")
        {
            item->getDynamics().setFadeOut(0.25);
            item->getDynamics().setFadeOutEnd(-0.25);
            engine->getPlayListScheduler()->commitPlayListData();

            bounceConfig->playListItem = item;

            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce();

            THEN("the exported file spans the audible length and carries the fade")
            {
                auto buffer = audioFileToAudioBuffer(bounceConfig->fileName);
                auto audibleSamples = static_cast<int>(sr * 1.25);
                REQUIRE(buffer.getNumSamples() >= audibleSamples);

                examineFadeOutEnvelope(buffer,
                                       static_cast<int>(sr * 0.75),
                                       audibleSamples,
                                       bounceConfig->blockSize);
            }
        }
    }

    // the config must not keep the item alive past the engine teardown
    bounceConfig->playListItem = nullptr;
    engine = nullptr;

    if (bounceConfig->fileName.existsAsFile())
        bounceConfig->fileName.deleteFile();

    if (inputFile.existsAsFile())
        inputFile.deleteFile();

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO("a tail extension past the end of the source file is silent", "[engine][dsp][fade]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto sr = 44100.0;
    auto inputFile = generateDcOffsetAudioFile(2.0);

    auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
    bounceConfig->fileName = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/fade-extension-eof.wav"));
    bounceConfig->sampleRate = sr;
    bounceConfig->blockSize = 512;
    bounceConfig->numChannels = 1;

    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    GIVEN("a full-file clip whose fade-out end reaches past the file")
    {
        store->open(inputFile, nullptr);

        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        auto item = track->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(item != nullptr);

        // fractions of the 2 second region: -0.25 = half a second past the end
        item->getDynamics().setFadeOutEnd(-0.25);
        engine->getPlayListScheduler()->commitPlayListData();

        WHEN("bouncing the session")
        {
            bounceConfig->lengthSeconds = engine->getPlayListScheduler()->getTotalLength(audium::seconds);
            REQUIRE(bounceConfig->lengthSeconds == Catch::Approx(2.5));

            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce();

            THEN("the clip plays to the file end and the extension is silence")
            {
                auto buffer = audioFileToAudioBuffer(bounceConfig->fileName);
                auto totalSamples = static_cast<int>(sr * 2.5);
                REQUIRE(buffer.getNumSamples() >= totalSamples);

                for (auto s = 0; s < totalSamples; s++) {
                    auto val = buffer.getSample(0, s);
                    INFO("sample " << s << " = " << val);
                    REQUIRE(std::isfinite(val));
                }

                auto slack = bounceConfig->blockSize;
                auto tailStart = static_cast<int>(sr * 2.0) + slack;
                auto tail = buffer.getMagnitude(0, tailStart, totalSamples - tailStart);
                INFO("tail magnitude " << tail);
                REQUIRE(tail == Catch::Approx(0.0).margin(0.01));
            }
        }
    }

    engine = nullptr;

    if (bounceConfig->fileName.existsAsFile())
        bounceConfig->fileName.deleteFile();

    if (inputFile.existsAsFile())
        inputFile.deleteFile();

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO("two clips crossfade over a cut at constant gain", "[engine][dsp][fade][level]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto sr = 44100.0;
    auto inputFile = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/noise0dB.wav"));
    REQUIRE(inputFile.existsAsFile());

    auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
    bounceConfig->fileName = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/xfade-noise.wav"));
    bounceConfig->sampleRate = sr;
    bounceConfig->blockSize = 512;
    bounceConfig->numChannels = 1;

    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    GIVEN("a 1 second 0 dB noise file split into two clips at 0.5 s")
    {
        store->open(inputFile, nullptr);

        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        auto itemA = track->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(itemA != nullptr);

        // clone before trimming: the clone gets its own region, so the two
        // clips can carry different file windows
        auto itemB = track->getPlayListContainer()->clonePlayListItem(itemA);
        REQUIRE(itemB != nullptr);

        itemA->setLength(0.5, audium::seconds);
        itemB->setAbsoluteStartPosition(0.5, audium::seconds);
        REQUIRE(itemA->getRegionData(audium::seconds).getEnd() == Catch::Approx(0.5));
        REQUIRE(itemB->getRegionData(audium::seconds).getStart() == Catch::Approx(0.5));
        REQUIRE(itemB->getAbsolutePosition(audium::seconds) == Catch::Approx(0.5));

        // 100 ms crossfade over the cut: A rings out past its end while B
        // fades in - the overlap plays the SAME file samples twice, so with
        // LINEAR curves the fades must sum to exactly unity gain
        itemA->getDynamics().setFadeOutEnd(-0.2);  // fraction of the 0.5 s region
        itemA->getDynamics().setFadeOutCurve(1.0);
        itemB->getDynamics().setFadeIn(0.2);
        itemB->getDynamics().setFadeInCurve(1.0);

        engine->getPlayListScheduler()->commitPlayListData();

        WHEN("bouncing the session")
        {
            bounceConfig->lengthSeconds = engine->getPlayListScheduler()->getTotalLength(audium::seconds);
            REQUIRE(bounceConfig->lengthSeconds == Catch::Approx(1.0));

            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce();

            THEN("the magnitude tracks the 0 dB source - no dip, no bump")
            {
                auto output = audioFileToAudioBuffer(bounceConfig->fileName);
                auto source = audioFileToAudioBuffer(inputFile);
                auto totalSamples = static_cast<int>(sr * 1.0);
                REQUIRE(output.getNumSamples() >= totalSamples);
                REQUIRE(source.getNumSamples() >= totalSamples);

                // no NaN / Inf anywhere
                for (auto s = 0; s < totalSamples; s++) {
                    auto val = output.getSample(0, s);
                    INFO("sample " << s << " = " << val);
                    REQUIRE(std::isfinite(val));
                }

                // never louder than the source (an equal-power crossfade of
                // identical material would bump the overlap by +3 dB)
                auto sourceMagnitude = source.getMagnitude(0, 0, totalSamples);
                auto outputMagnitude = output.getMagnitude(0, 0, totalSamples);
                INFO("source " << sourceMagnitude << " output " << outputMagnitude);
                REQUIRE(outputMagnitude <= sourceMagnitude + 0.02f);

                // window by window (50 ms), the level must match the source -
                // a broken overlap would dip inside the crossfade. skip the
                // resampler warm-up at the very start.
                auto window = static_cast<int>(sr * 0.05);
                for (auto start = window; start + window <= totalSamples; start += window) {
                    auto magOut = output.getMagnitude(0, start, window);
                    auto magIn = source.getMagnitude(0, start, window);
                    INFO("window at " << start << " out " << magOut << " in " << magIn);
                    REQUIRE(magOut == Catch::Approx(magIn).margin(0.05));
                }
            }
        }
    }

    engine = nullptr;

    if (bounceConfig->fileName.existsAsFile())
        bounceConfig->fileName.deleteFile();

    // noise0dB.wav is a tracked fixture - do not delete it

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO("uncorrelated clips crossfade at constant power", "[engine][dsp][fade][level]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto sr = 44100.0;
    auto inputFile = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/noise0dB.wav"));
    REQUIRE(inputFile.existsAsFile());

    auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
    bounceConfig->fileName = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/xfade-noise-power.wav"));
    bounceConfig->sampleRate = sr;
    bounceConfig->blockSize = 512;
    bounceConfig->numChannels = 1;

    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    GIVEN("two clips whose 100 ms overlap plays different noise segments")
    {
        store->open(inputFile, nullptr);

        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        auto itemA = track->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(itemA != nullptr);

        auto itemB = track->getPlayListContainer()->clonePlayListItem(itemA);
        REQUIRE(itemB != nullptr);

        // A: file [0, 0.4] at timeline 0, ringing out over file [0.4, 0.5].
        // B: file [0.5, 1.0] at timeline 0.4. the overlap [0.4, 0.5] mixes
        // file [0.4, 0.5] with file [0.5, 0.6] - independent noise, so the
        // POWERS add and the default equal-power curves must keep the summed
        // power constant (a linear pair would dip -3 dB at the centre)
        itemA->setLength(0.4, audium::seconds);
        itemA->getDynamics().setFadeOutEnd(-0.25); // fraction of the 0.4 s region

        itemB->setAbsoluteStartPosition(0.5, audium::seconds);
        itemB->setAbsolutePosition(0.4, audium::seconds);
        itemB->getDynamics().setFadeIn(0.2);       // fraction of the 0.5 s region
        REQUIRE(itemB->getRegionData(audium::seconds).getStart() == Catch::Approx(0.5));

        engine->getPlayListScheduler()->commitPlayListData();

        WHEN("bouncing the session")
        {
            bounceConfig->lengthSeconds = engine->getPlayListScheduler()->getTotalLength(audium::seconds);
            REQUIRE(bounceConfig->lengthSeconds == Catch::Approx(0.9));

            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce();

            THEN("the RMS stays flat through the crossfade")
            {
                auto output = audioFileToAudioBuffer(bounceConfig->fileName);
                auto source = audioFileToAudioBuffer(inputFile);
                auto totalSamples = static_cast<int>(sr * 0.9);
                REQUIRE(output.getNumSamples() >= totalSamples);

                for (auto s = 0; s < totalSamples; s++) {
                    auto val = output.getSample(0, s);
                    INFO("sample " << s << " = " << val);
                    REQUIRE(std::isfinite(val));
                }

                auto sourceRms = source.getRMSLevel(0, 0, source.getNumSamples());
                REQUIRE(sourceRms > 0.1f);

                // 25 ms sub-windows across the overlap [0.4, 0.5] and its
                // shoulders: constant power = source RMS within tolerance.
                // -3 dB (0.707x, linear dip) or +3 dB (1.41x) would fail.
                auto window = static_cast<int>(sr * 0.025);
                for (auto start = static_cast<int>(sr * 0.35);
                     start + window <= static_cast<int>(sr * 0.55);
                     start += window) {
                    auto rms = output.getRMSLevel(0, start, window);
                    INFO("window at " << start << " rms " << rms << " source " << sourceRms);
                    REQUIRE(rms == Catch::Approx(sourceRms).margin(sourceRms * 0.15));
                }
            }
        }
    }

    engine = nullptr;

    if (bounceConfig->fileName.existsAsFile())
        bounceConfig->fileName.deleteFile();

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO("splitting a clip preserves its edge fades", "[engine][fade]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto inputFile = generateDcOffsetAudioFile(2.0);
    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    GIVEN("a 2 second clip with full fade dynamics")
    {
        store->open(inputFile, nullptr);

        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        auto item = track->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(item != nullptr);

        // fractions of the 2 s region
        item->getDynamics().setFadeIn(0.25);        // 0.5 s
        item->getDynamics().setFadeInStart(0.1);    // 0.2 s silent head
        item->getDynamics().setFadeInCurve(1.5);
        item->getDynamics().setFadeOut(0.25);       // 0.5 s
        item->getDynamics().setFadeOutEnd(-0.1);    // 0.2 s extension
        item->getDynamics().setFadeOutCurve(2.0);

        WHEN("the clip is split at its middle")
        {
            engine->getAudioTrackContainer()->getAudioRegionAdapter().splitRegions(1.0, audium::seconds);

            auto items = track->getPlayListContainer()->getPlayListItems();
            REQUIRE(items.size() == 2);

            THEN("the left piece keeps the fade-in family, and only that")
            {
                auto& dynamics = items[0]->getDynamics();
                REQUIRE(dynamics.getFadeIn(audium::seconds) == Catch::Approx(0.5));
                REQUIRE(dynamics.getFadeInStart(audium::seconds) == Catch::Approx(0.2));
                REQUIRE(dynamics.getFadeInCurve() == Catch::Approx(1.5));
                REQUIRE(dynamics.getFadeOut(audium::seconds) == Catch::Approx(0.0));
                REQUIRE(dynamics.getFadeOutEnd(audium::seconds) == Catch::Approx(0.0));
            }

            THEN("the right piece keeps the fade-out family, and only that")
            {
                auto& dynamics = items[1]->getDynamics();
                REQUIRE(dynamics.getFadeOut(audium::seconds) == Catch::Approx(0.5));
                REQUIRE(dynamics.getFadeOutEnd(audium::seconds) == Catch::Approx(-0.2));
                REQUIRE(dynamics.getFadeOutCurve() == Catch::Approx(2.0));
                REQUIRE(dynamics.getFadeIn(audium::seconds) == Catch::Approx(0.0));
                REQUIRE(dynamics.getFadeInStart(audium::seconds) == Catch::Approx(0.0));
            }
        }

        WHEN("the clip is split inside its fade-in")
        {
            engine->getAudioTrackContainer()->getAudioRegionAdapter().splitRegions(0.3, audium::seconds);

            auto items = track->getPlayListContainer()->getPlayListItems();
            REQUIRE(items.size() == 2);

            THEN("the left piece's fade-in is clamped to its length")
            {
                auto& dynamics = items[0]->getDynamics();
                REQUIRE(dynamics.getFadeIn(audium::seconds) == Catch::Approx(0.3));
                REQUIRE(dynamics.getFadeInStart(audium::seconds) == Catch::Approx(0.2));
            }
        }
    }

    engine = nullptr;

    if (inputFile.existsAsFile())
        inputFile.deleteFile();

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO("clip dynamics only touch the active sub-block region", "[engine][dsp][fade]")
{
    auto sr = 44100.0;
    auto blockSize = 512;
    auto lengthSeconds = 2.0;
    auto fadeOutSeconds = 1.0;
    auto totalSamples = static_cast<int>(sr * lengthSeconds);

    GIVEN("a transport source over a 2 second DC signal with a 1 second fade out")
    {
        AudioBuffer<float> dcBuffer(1, totalSamples);
        for (auto s = 0; s < totalSamples; s++)
            dcBuffer.setSample(0, s, 1.f);

        // reference: the same clip rendered with one full-buffer call per block
        AudioBuffer<float> reference(1, totalSamples);
        {
            MemoryAudioSource referenceSource(dcBuffer, false);
            audium::ClipTransportSource referenceTransport;
            referenceTransport.setSource(&referenceSource, 0, nullptr, 0.0, 1);
            referenceTransport.prepareToPlay(blockSize, sr);
            referenceTransport.resetClipGain();
            referenceTransport.clearFadeIn();
            referenceTransport.setFadeOutRamp(fadeOutSeconds, lengthSeconds - fadeOutSeconds, true);
            referenceTransport.start();

            AudioBuffer<float> block(1, blockSize);
            auto rendered = 0;
            while (rendered < totalSamples) {
                auto numSamples = jmin(blockSize, totalSamples - rendered);
                AudioSourceChannelInfo info(&block, 0, numSamples);
                info.clearActiveBufferRegion();
                referenceTransport.getNextAudioBlock(info);
                reference.copyFrom(0, rendered, block, 0, 0, numSamples);
                rendered += numSamples;
            }
        }

        MemoryAudioSource memorySource(dcBuffer, false);
        audium::ClipTransportSource transportSource;
        transportSource.setSource(&memorySource, 0, nullptr, 0.0, 1);
        transportSource.prepareToPlay(blockSize, sr);
        transportSource.resetClipGain();
        transportSource.clearFadeIn();
        transportSource.setFadeOutRamp(fadeOutSeconds, lengthSeconds - fadeOutSeconds, true);
        transportSource.start();

        WHEN("each callback is split into two sub-block calls at a non-zero start sample")
        {
            // mimics VoiceSource::getNextAudioBlock, which splits a
            // callback into part 1 / part 2 on loop wrap and clip end
            const auto offset = 128;
            const auto sentinel = -2.f;

            AudioBuffer<float> output(1, totalSamples);
            AudioBuffer<float> block(1, offset + blockSize);

            auto rendered = 0;
            while (rendered < totalSamples) {
                auto numSamples = jmin(blockSize, totalSamples - rendered);
                auto part1 = numSamples / 2;
                auto part2 = numSamples - part1;

                for (auto s = 0; s < block.getNumSamples(); s++)
                    block.setSample(0, s, sentinel);

                AudioSourceChannelInfo info1(&block, offset, part1);
                info1.clearActiveBufferRegion();
                transportSource.getNextAudioBlock(info1);

                if (part2 > 0) {
                    AudioSourceChannelInfo info2(&block, offset + part1, part2);
                    info2.clearActiveBufferRegion();
                    transportSource.getNextAudioBlock(info2);
                }

                // samples before the active region must keep the sentinel
                for (auto s = 0; s < offset; s++) {
                    INFO("sentinel sample " << s << " at block start " << rendered);
                    REQUIRE(block.getSample(0, s) == sentinel);
                }

                output.copyFrom(0, rendered, block, 0, offset, numSamples);
                rendered += numSamples;
            }

            THEN("the rendered envelope matches the single-call reference sample for sample")
            {
                for (auto s = 0; s < totalSamples; s++) {
                    INFO("sample " << s << " split " << output.getSample(0, s)
                                   << " reference " << reference.getSample(0, s));
                    REQUIRE(output.getSample(0, s)
                            == Catch::Approx(reference.getSample(0, s)).margin(1e-6));
                }

                auto fadeStartSample = static_cast<int>(sr * (lengthSeconds - fadeOutSeconds));
                examineFadeOutEnvelope(output, fadeStartSample, totalSamples, blockSize);
            }
        }
    }
}
