//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Export/AudioExporter.h"
#include "Engine/Export/ExportAudioConfig.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Region/AudioRegion.h"

#include "TestUtils.h"

// Per-clip re-pitch (varispeed): a speed ratio s plays the source s times
// faster, one octave up per doubling, on 1/s of the timeline. These
// scenarios cover the timeline model, the resampled audio, and persistence.

using namespace audium;

namespace {

struct Fixture {
    std::shared_ptr<AudiumEngine> engine;
    juce::File inputFile;
    juce::File bounceFile;

    std::shared_ptr<PlayListItem> item() const
    {
        return engine->getAudioTrackContainer()->getAudioTrack(0)
            ->getPlayListContainer()->getPlayListItem(0);
    }

    void commit() const { engine->getPlayListScheduler()->commitPlayListData(); }

    juce::AudioBuffer<float> bounce(double positionSeconds = 0.0, double lengthSeconds = -1.0)
    {
        auto config = std::make_shared<ExportAudioConfig>();
        config->fileName = bounceFile;
        config->sampleRate = 44100.0;
        config->blockSize = 512;
        config->numChannels = 1;
        config->positionSeconds = positionSeconds;
        config->lengthSeconds = lengthSeconds >= 0.0
            ? lengthSeconds
            : engine->getPlayListScheduler()->getTotalLength(audium::seconds) - positionSeconds;

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
    fixture.inputFile = audioFile;
    fixture.bounceFile = juce::File(juce::String(CURRENT_SOURCE_DIR) + "/TestFiles/clip-speed-out.wav");

    REQUIRE(audioFile.existsAsFile());
    REQUIRE(fixture.engine->getProjectFileStore()->open(audioFile, nullptr));
    REQUIRE(fixture.item() != nullptr);

    return fixture;
}

/// The ramp's rise per sample between two probe times chosen inside one
/// saw cycle - endpoint-based, so the resampler's interpolation ripple at
/// cycle boundaries cannot pollute it.
double rampSlope(const juce::AudioBuffer<float>& buffer, double fromSeconds, double toSeconds)
{
    const auto from = static_cast<int>(fromSeconds * 44100.0);
    const auto to = static_cast<int>(toSeconds * 44100.0);
    REQUIRE(to > from);
    REQUIRE(to < buffer.getNumSamples());

    return (buffer.getSample(0, to) - buffer.getSample(0, from)) / static_cast<double>(to - from);
}

} // namespace

SCENARIO("clip speed scales the timeline model", "[engine][clipspeed]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture(createSlowSawTwoSecondsAudioFile());
        auto item = fixture.item();
        const auto sourceLength = item->getRegionData(audium::seconds).getLength();
        REQUIRE(sourceLength == Catch::Approx(2.0));

        WHEN("the clip runs at half speed")
        {
            item->setSpeedRatio(0.5);
            fixture.commit();

            THEN("its timeline extent doubles while the source window is untouched")
            {
                REQUIRE(item->getDurationTime(audium::seconds) == Catch::Approx(4.0));
                REQUIRE(item->getAbsolutePositionRange(audium::seconds).getLength() == Catch::Approx(4.0));
                REQUIRE(item->getRegionData(audium::seconds).getLength() == Catch::Approx(2.0));
                REQUIRE(fixture.engine->getPlayListScheduler()->getTotalLength(audium::seconds)
                        == Catch::Approx(4.0));
            }

            THEN("timeline positions map to source positions at the speed ratio")
            {
                // 1 timeline second into the clip = 0.5 source seconds
                REQUIRE(item->absoluteToLocalPosition(1.0, audium::seconds) == Catch::Approx(0.5));
            }
        }

        WHEN("the ratio is set outside the supported range")
        {
            item->setSpeedRatio(10.0);
            REQUIRE(item->getSpeedRatio() == Catch::Approx(PlayListItem::maxSpeedRatio));

            item->setSpeedRatio(0.01);
            REQUIRE(item->getSpeedRatio() == Catch::Approx(PlayListItem::minSpeedRatio));
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("a half-speed clip plays twice as long at half the rate", "[engine][clipspeed]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        // two saw cycles, no padding: a steadily rising ramp with one
        // discontinuity per source second
        auto fixture = makeFixture(createSlowSawAudioFile(2, false, false));

        // reference at 1.0x: within the first cycle
        const auto referenceSlope = [&fixture] {
            fixture.commit();
            return rampSlope(fixture.bounce(), 0.1, 0.8);
        }();
        REQUIRE(referenceSlope > 0.0);

        WHEN("the clip is bounced at half speed")
        {
            fixture.item()->setSpeedRatio(0.5);
            fixture.commit();
            auto buffer = fixture.bounce();

            THEN("the bounce is twice as long")
            {
                REQUIRE(buffer.getNumSamples() == Catch::Approx(4.0 * 44100.0).margin(512));
            }

            THEN("the ramp rises at half the rate - an octave down")
            {
                // first stretched cycle spans 0..2 s
                REQUIRE(rampSlope(buffer, 0.2, 1.6) == Catch::Approx(referenceSlope * 0.5).epsilon(0.02));
            }
        }

        WHEN("the clip is bounced at double speed")
        {
            fixture.item()->setSpeedRatio(2.0);
            fixture.commit();
            auto buffer = fixture.bounce();

            THEN("the bounce is half as long and rises twice as fast")
            {
                REQUIRE(buffer.getNumSamples() == Catch::Approx(1.0 * 44100.0).margin(512));
                // first compressed cycle spans 0..0.5 s
                REQUIRE(rampSlope(buffer, 0.05, 0.4) == Catch::Approx(referenceSlope * 2.0).epsilon(0.02));
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("clip speed changes duration but not amplitude", "[engine][clipspeed]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture(generateDcOffsetAudioFile(1.0));
        fixture.item()->setSpeedRatio(0.5);
        fixture.commit();

        auto buffer = fixture.bounce();
        const auto warmUp = 256;

        THEN("two timeline seconds of unity DC come out of one source second")
        {
            REQUIRE(buffer.getNumSamples() == Catch::Approx(2.0 * 44100.0).margin(512));

            for (auto i = warmUp; i < buffer.getNumSamples() - warmUp; i += 483)
                REQUIRE(buffer.getSample(0, i) == Catch::Approx(1.0).margin(0.01));
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("starting playback inside a stretched clip seeks the speed-scaled file position",
         "[engine][clipspeed]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        // 2 s source at half speed = 4 s timeline; bouncing the second half
        // must play source seconds 1..2, not run out of material
        auto fixture = makeFixture(createSlowSawAudioFile(2, false, false));
        fixture.item()->setSpeedRatio(0.5);
        fixture.commit();

        auto buffer = fixture.bounce(2.0, 2.0);

        THEN("the rendered tail matches the speed-scaled source mapping")
        {
            REQUIRE(buffer.getNumSamples() == Catch::Approx(2.0 * 44100.0).margin(512));

            // output t maps to source 1.0 + t * 0.5; the saw's value there
            // is -1 + 2 * cycleFraction
            const auto probe = static_cast<int>(0.5 * 44100.0);   // source 1.25 s -> -0.5
            REQUIRE(buffer.getSample(0, probe) == Catch::Approx(-0.5).margin(0.02));

            const auto probeLate = static_cast<int>(1.5 * 44100.0); // source 1.75 s -> +0.5
            REQUIRE(buffer.getSample(0, probeLate) == Catch::Approx(0.5).margin(0.02));
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("a stretched item bounce is speed-scaled", "[engine][clipspeed]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture(generateDcOffsetAudioFile(1.0));
        auto item = fixture.item();
        item->setSpeedRatio(0.5);
        fixture.commit();

        auto config = std::make_shared<ExportAudioConfig>();
        config->fileName = fixture.bounceFile;
        config->sampleRate = 44100.0;
        config->blockSize = 512;
        config->playListItem = item;

        AudioExporter(*fixture.engine, config).bounce();
        auto buffer = audioFileToAudioBuffer(fixture.bounceFile);

        THEN("the item bounce covers source length / speed")
        {
            REQUIRE(buffer.getNumSamples() == Catch::Approx(2.0 * 44100.0).margin(512));
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("clip speed persists and resets like the other item state", "[engine][clipspeed]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture(createSlowSawTwoSecondsAudioFile());
        auto item = fixture.item();

        json plain;
        REQUIRE(item->writeToJson(plain));

        THEN("the default speed writes no key, so old projects stay identical")
        {
            REQUIRE_FALSE(plain.contains("speed_ratio"));
        }

        WHEN("a speed is set and the item state round-trips")
        {
            item->setSpeedRatio(0.5);

            json withSpeed;
            REQUIRE(item->writeToJson(withSpeed));
            REQUIRE(withSpeed.at("speed_ratio").get<double>() == Catch::Approx(0.5));

            item->setSpeedRatio(2.0);
            REQUIRE(item->readFromJson(withSpeed, false));

            THEN("the stored speed comes back")
            {
                REQUIRE(item->getSpeedRatio() == Catch::Approx(0.5));
            }

            THEN("restoring a speedless state resets to 1.0 - undo reuses items")
            {
                REQUIRE(item->readFromJson(plain, false));
                REQUIRE(item->getSpeedRatio() == Catch::Approx(1.0));
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("fades on a stretched clip scale with the audio", "[engine][clipspeed]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture(generateDcOffsetAudioFile(1.0));
        auto item = fixture.item();

        // half the source faded in; at half speed that ramp lasts one
        // timeline second
        item->getDynamics().setFadeIn(0.5);
        item->setSpeedRatio(0.5);
        fixture.commit();

        auto buffer = fixture.bounce();

        THEN("the ramp midpoint sits half a timeline second in, at equal-power gain")
        {
            const auto midRamp = static_cast<int>(0.5 * 44100.0);
            REQUIRE(buffer.getSample(0, midRamp) == Catch::Approx(std::sqrt(0.5)).margin(0.05));

            // past the (stretched) ramp: unity
            const auto pastRamp = static_cast<int>(1.5 * 44100.0);
            REQUIRE(buffer.getSample(0, pastRamp) == Catch::Approx(1.0).margin(0.02));
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("splitting and cloning a stretched clip preserve its speed", "[engine][clipspeed]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture(createSlowSawTwoSecondsAudioFile());
        auto container = fixture.engine->getAudioTrackContainer();
        auto track = container->getAudioTrack(0);
        auto playList = track->getPlayListContainer();

        fixture.item()->setSpeedRatio(0.5);   // 2 s source -> 4 s timeline

        WHEN("the clip is split in the middle of its timeline extent")
        {
            container->getAudioRegionAdapter().splitRegions(2.0, audium::seconds);

            THEN("both pieces keep the speed, and their windows add up")
            {
                REQUIRE(playList->getPlayListItems().size() == 2);
                auto first = playList->getPlayListItem(0);
                auto second = playList->getPlayListItem(1);

                REQUIRE(first->getSpeedRatio() == Catch::Approx(0.5));
                REQUIRE(second->getSpeedRatio() == Catch::Approx(0.5));

                // each piece: half the source, twice that on the timeline
                REQUIRE(first->getRegionData(audium::seconds).getLength() == Catch::Approx(1.0));
                REQUIRE(second->getRegionData(audium::seconds).getLength() == Catch::Approx(1.0));
                REQUIRE(second->getRegionData(audium::seconds).getStart()
                        == Catch::Approx(first->getRegionData(audium::seconds).getEnd()));

                // timeline: seamless halves of the original extent
                REQUIRE(first->getAbsolutePositionRange(audium::seconds).getEnd() == Catch::Approx(2.0));
                REQUIRE(second->getAbsolutePositionRange(audium::seconds).getStart() == Catch::Approx(2.0));
                REQUIRE(second->getAbsolutePositionRange(audium::seconds).getEnd() == Catch::Approx(4.0));
            }
        }

        WHEN("the clip is cloned")
        {
            auto clone = playList->clonePlayListItem(fixture.item());
            REQUIRE(clone != nullptr);

            THEN("the clone keeps the speed")
            {
                REQUIRE(clone->getSpeedRatio() == Catch::Approx(0.5));
            }
        }

        WHEN("a pending speed change is cancelled")
        {
            auto undoManager = container->getUndoManager();
            const auto undoableBefore = undoManager->canUndo();

            fixture.item()->onDragStart();
            fixture.item()->setSpeedRatio(2.0);
            fixture.item()->onDragCancel();

            THEN("the speed is rolled back and no undo entry appears")
            {
                REQUIRE(fixture.item()->getSpeedRatio() == Catch::Approx(0.5));
                REQUIRE(undoManager->canUndo() == undoableBefore);
                REQUIRE_FALSE(undoManager->canRedo());
            }
        }

        WHEN("a speed change is undone and redone")
        {
            auto undoManager = container->getUndoManager();

            fixture.item()->onDragStart();
            fixture.item()->setSpeedRatio(2.0);
            fixture.item()->onDragEnd();

            REQUIRE(undoManager->canUndo());
            undoManager->undo();

            THEN("undo restores the previous speed and redo re-applies it")
            {
                REQUIRE(fixture.item()->getSpeedRatio() == Catch::Approx(0.5));

                undoManager->redo();
                REQUIRE(fixture.item()->getSpeedRatio() == Catch::Approx(2.0));
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
