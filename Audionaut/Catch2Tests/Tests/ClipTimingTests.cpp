//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Export/AudioExporter.h"
#include "Engine/Export/ExportAudioConfig.h"
#include "Engine/Group/AudioRegionAdapter.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Project/ProjectFileStore.h"

#include "TestUtils.h"

// Clip boundaries must land on the exact output sample whatever rate the
// engine renders at: a clip placed at p seconds starts at round(p * sr)
// and its last sample is round((p + length) * sr) - 1. These scenarios
// bounce DC and ramp sources at several processing rates - with the
// source at the same rate (no resampling, so every sample is checkable)
// and with a 44.1 kHz source that the chain has to resample - and check
// the edges sample by sample.

using namespace audium;

namespace {

constexpr double processingRates[] = { 44100.0, 48000.0, 88200.0, 96000.0 };

/// The output sample a timeline position maps to at a given rate.
int sampleAt(double seconds, double sampleRate)
{
    return static_cast<int>(std::round(seconds * sampleRate));
}

struct Fixture {
    std::shared_ptr<AudiumEngine> engine;
    juce::File bounceFile;

    std::shared_ptr<PlayListItem> item(int index = 0) const
    {
        return engine->getAudioTrackContainer()->getAudioTrack(0)
            ->getPlayListContainer()->getPlayListItem(index);
    }

    void commit() const { engine->getPlayListScheduler()->commitPlayListData(); }

    juce::AudioBuffer<float> bounce(double sampleRate, double positionSeconds, double lengthSeconds,
                                    int blockSize = 512, std::shared_ptr<PlayListItem> exportItem = nullptr)
    {
        auto config = std::make_shared<ExportAudioConfig>();
        config->fileName = bounceFile;
        config->sampleRate = sampleRate;
        config->blockSize = blockSize;
        config->numChannels = 1;
        config->bitDepth = 32;
        config->positionSeconds = positionSeconds;
        config->lengthSeconds = lengthSeconds;
        config->playListItem = exportItem;

        AudioExporter(*engine, config).bounce();
        return audioFileToAudioBuffer(bounceFile);
    }

    ~Fixture()
    {
        engine = nullptr;
        bounceFile.deleteFile();
    }
};

Fixture makeFixture(const juce::File& audioFile)
{
    Fixture fixture;
    fixture.engine = AudiumFactory::createAudiumEngine();
    fixture.bounceFile = juce::File(juce::String(CURRENT_SOURCE_DIR) + "/TestFiles/clip-timing-out.wav");

    REQUIRE(audioFile.existsAsFile());
    REQUIRE(fixture.engine->getProjectFileStore()->open(audioFile, nullptr));
    REQUIRE(fixture.item() != nullptr);

    return fixture;
}

int firstSampleAbove(const juce::AudioBuffer<float>& buffer, float threshold)
{
    for (auto i = 0; i < buffer.getNumSamples(); ++i)
        if (std::abs(buffer.getSample(0, i)) > threshold)
            return i;
    return -1;
}

int lastSampleAbove(const juce::AudioBuffer<float>& buffer, float threshold)
{
    for (auto i = buffer.getNumSamples() - 1; i >= 0; --i)
        if (std::abs(buffer.getSample(0, i)) > threshold)
            return i;
    return -1;
}

/// Every sample in [from, to) is within margin of value; reports the
/// first offender.
void requireConstant(const juce::AudioBuffer<float>& buffer, int from, int to, float value, float margin)
{
    REQUIRE(from >= 0);
    REQUIRE(to <= buffer.getNumSamples());
    for (auto i = from; i < to; ++i) {
        INFO("sample " << i << " of " << buffer.getNumSamples());
        REQUIRE(buffer.getSample(0, i) == Catch::Approx(value).margin(margin));
    }
}

/// A DC clip of clipLength seconds placed at position: silence, the
/// exact run of ones, silence - each edge on its own sample.
void requireDcEdges(const juce::AudioBuffer<float>& buffer, double sampleRate,
                    double position, double clipLength)
{
    const auto start = sampleAt(position, sampleRate);
    const auto end = sampleAt(position + clipLength, sampleRate);

    INFO("rate " << sampleRate << ", clip at " << position << " s");
    REQUIRE(firstSampleAbove(buffer, 0.0f) == start);
    REQUIRE(lastSampleAbove(buffer, 0.0f) == end - 1);

    requireConstant(buffer, 0, start, 0.0f, 0.0f);
    requireConstant(buffer, start, end, 1.0f, 1.0e-5f);
    requireConstant(buffer, end, buffer.getNumSamples(), 0.0f, 0.0f);
}

} // namespace

SCENARIO("a clip starts and ends on the exact sample at every processing rate", "[engine][timing][samplerate]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    for (const auto rate : processingRates)
    {
        DYNAMIC_SECTION("processing at " << static_cast<int>(rate) << " Hz")
        {
            // position 0 (nothing to skip), a half second, a fraction that
            // hits neither a block nor a sample boundary, and a start that
            // falls exactly on a 512-sample block boundary (100 blocks in)
            for (const auto position : { 0.0, 0.5, 0.3217, 51200.0 / rate })
            {
                DYNAMIC_SECTION("a one-second DC clip placed at " << position << " s")
                {
                    auto fixture = makeFixture(generateDcOffsetAudioFile(1.0, rate));
                    fixture.item()->setAbsolutePosition(position, audium::seconds);
                    fixture.commit();

                    REQUIRE(fixture.item()->getAbsolutePosition(audium::seconds) == Catch::Approx(position));

                    WHEN("it is bounced in 512-sample blocks")
                    {
                        auto buffer = fixture.bounce(rate, 0.0, position + 1.5);

                        THEN("the ones run from round(p * sr) up to round((p + 1) * sr) - 1")
                        {
                            requireDcEdges(buffer, rate, position, 1.0);
                        }
                    }

                    WHEN("it is bounced in odd-sized blocks")
                    {
                        auto buffer = fixture.bounce(rate, 0.0, position + 1.5, 1000);

                        THEN("the edges do not move")
                        {
                            requireDcEdges(buffer, rate, position, 1.0);
                        }
                    }
                }
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("a trimmed clip plays exactly its source window at every processing rate", "[engine][timing][samplerate]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    for (const auto rate : processingRates)
    {
        DYNAMIC_SECTION("processing at " << static_cast<int>(rate) << " Hz")
        {
            // a two-second ramp: sample s carries genSaw(s, n), so the
            // bounce tells which source sample landed on which output sample
            const auto numSourceSamples = static_cast<int>(2.0 * rate);
            auto fixture = makeFixture(createRampAudioFile(2.0, rate));
            auto item = fixture.item();

            // 0.3 * 44100 is 13229.999... in floating point: a truncating
            // seek would start one source sample early
            const auto regionStart = 0.3;
            const auto regionEnd = 1.7;
            const auto position = 0.5;
            item->setRegionData({ regionStart, regionEnd }, audium::seconds);
            item->setAbsolutePosition(position, audium::seconds);
            fixture.commit();

            WHEN("the clip is bounced")
            {
                auto buffer = fixture.bounce(rate, 0.0, 3.0);

                THEN("the first output sample of the clip is the region's first source sample")
                {
                    const auto start = sampleAt(position, rate);
                    const auto end = sampleAt(position + (regionEnd - regionStart), rate);
                    const auto sourceStart = sampleAt(regionStart, rate);

                    requireConstant(buffer, 0, start, 0.0f, 0.0f);

                    for (auto i = start; i < end; ++i) {
                        INFO("output sample " << i << " (clip sample " << i - start << ")");
                        REQUIRE(buffer.getSample(0, i)
                                == Catch::Approx(genSaw(sourceStart + (i - start), numSourceSamples)).margin(1.0e-5));
                    }

                    requireConstant(buffer, end, buffer.getNumSamples(), 0.0f, 0.0f);
                }
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("a bounce that starts inside a clip picks up on the exact sample", "[engine][timing][samplerate]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    for (const auto rate : processingRates)
    {
        DYNAMIC_SECTION("processing at " << static_cast<int>(rate) << " Hz")
        {
            const auto numSourceSamples = static_cast<int>(1.0 * rate);
            auto fixture = makeFixture(createRampAudioFile(1.0, rate));
            fixture.commit();

            WHEN("the bounce window starts 0.3 s into the clip and runs past its end")
            {
                const auto from = 0.3;
                auto buffer = fixture.bounce(rate, from, 1.0);

                THEN("output sample 0 is source sample round(0.3 * sr) and the clip ends where it should")
                {
                    const auto sourceStart = sampleAt(from, rate);
                    const auto end = sampleAt(1.0 - from, rate);

                    for (auto i = 0; i < end; ++i) {
                        INFO("output sample " << i);
                        REQUIRE(buffer.getSample(0, i)
                                == Catch::Approx(genSaw(sourceStart + i, numSourceSamples)).margin(1.0e-5));
                    }

                    requireConstant(buffer, end, buffer.getNumSamples(), 0.0f, 0.0f);
                }
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("exporting a single trimmed clip yields exactly its samples at every processing rate", "[engine][timing][samplerate][export]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    for (const auto rate : processingRates)
    {
        DYNAMIC_SECTION("processing at " << static_cast<int>(rate) << " Hz")
        {
            const auto numSourceSamples = static_cast<int>(2.0 * rate);
            auto fixture = makeFixture(createRampAudioFile(2.0, rate));
            auto item = fixture.item();

            const auto regionStart = 0.3;
            const auto regionEnd = 1.7;
            item->setRegionData({ regionStart, regionEnd }, audium::seconds);
            fixture.commit();

            WHEN("the clip alone is exported")
            {
                auto buffer = fixture.bounce(rate, 0.0, -1.0, 512, item);

                THEN("the file holds round(length * sr) samples, first to last source sample of the window")
                {
                    const auto sourceStart = sampleAt(regionStart, rate);
                    const auto numSamples = sampleAt(regionEnd, rate) - sourceStart;
                    REQUIRE(buffer.getNumSamples() == numSamples);

                    for (auto i = 0; i < numSamples; ++i) {
                        INFO("output sample " << i);
                        REQUIRE(buffer.getSample(0, i)
                                == Catch::Approx(genSaw(sourceStart + i, numSourceSamples)).margin(1.0e-5));
                    }
                }
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("clips split at an arbitrary position join without a gap or an overlap", "[engine][timing][samplerate]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    for (const auto rate : processingRates)
    {
        DYNAMIC_SECTION("processing at " << static_cast<int>(rate) << " Hz")
        {
            auto fixture = makeFixture(generateDcOffsetAudioFile(2.0, rate));
            auto& adapter = fixture.engine->getAudioTrackContainer()->getAudioRegionAdapter();

            // neither a block nor a sample boundary
            const auto splitAt = 0.7137;
            adapter.splitRegions(splitAt, audium::seconds);
            fixture.commit();

            auto playList = fixture.engine->getAudioTrackContainer()->getAudioTrack(0)->getPlayListContainer();
            REQUIRE(playList->getNumItems() == 2);

            WHEN("the two halves are bounced")
            {
                auto buffer = fixture.bounce(rate, 0.0, 2.5);

                THEN("the DC is unbroken across the seam and ends on the original last sample")
                {
                    requireDcEdges(buffer, rate, 0.0, 2.0);
                }
            }

            WHEN("the second half is moved away from the first")
            {
                auto second = fixture.item(1);
                const auto newPosition = 1.2;
                second->setAbsolutePosition(newPosition, audium::seconds);
                fixture.commit();
                auto buffer = fixture.bounce(rate, 0.0, 3.0);

                THEN("both halves keep their own sample-exact edges")
                {
                    const auto firstEnd = sampleAt(splitAt, rate);
                    const auto secondStart = sampleAt(newPosition, rate);
                    const auto secondEnd = sampleAt(newPosition + (2.0 - splitAt), rate);

                    requireConstant(buffer, 0, firstEnd, 1.0f, 1.0e-5f);
                    requireConstant(buffer, firstEnd, secondStart, 0.0f, 0.0f);
                    requireConstant(buffer, secondStart, secondEnd, 1.0f, 1.0e-5f);
                    requireConstant(buffer, secondEnd, buffer.getNumSamples(), 0.0f, 0.0f);
                }
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("a 44.1 kHz clip keeps its timeline edges when rendered at another rate", "[engine][timing][samplerate][resample]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    for (const auto rate : { 48000.0, 88200.0, 96000.0 })
    {
        DYNAMIC_SECTION("processing at " << static_cast<int>(rate) << " Hz")
        {
            for (const auto position : { 0.0, 0.3217 })
            {
                DYNAMIC_SECTION("a one-second 44.1 kHz DC clip placed at " << position << " s")
                {
                    auto fixture = makeFixture(generateDcOffsetAudioFile(1.0, 44100.0));
                    fixture.item()->setAbsolutePosition(position, audium::seconds);
                    fixture.commit();

                    auto buffer = fixture.bounce(rate, 0.0, position + 1.5);

                    THEN("the clip's first and last sample stay on the timeline")
                    {
                        const auto start = sampleAt(position, rate);
                        const auto end = sampleAt(position + 1.0, rate);

                        INFO("first above 0: " << firstSampleAbove(buffer, 0.0f)
                             << ", first above 0.5: " << firstSampleAbove(buffer, 0.5f)
                             << ", last above 0.5: " << lastSampleAbove(buffer, 0.5f)
                             << ", last above 0: " << lastSampleAbove(buffer, 0.0f)
                             << ", expected " << start << " .. " << end - 1);

                        REQUIRE(firstSampleAbove(buffer, 0.0f) == start);
                        REQUIRE(lastSampleAbove(buffer, 0.0f) == end - 1);
                        requireConstant(buffer, 0, start, 0.0f, 0.0f);
                        requireConstant(buffer, end, buffer.getNumSamples(), 0.0f, 0.0f);
                    }
                }
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
