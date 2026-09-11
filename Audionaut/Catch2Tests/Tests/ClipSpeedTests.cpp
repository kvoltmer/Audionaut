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
#include "Engine/PlayList/ClipSpeed.h"
#include "Engine/PlayList/ClipTempo.h"
#include "Engine/AudioSources/VoiceSource.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Analysis/AnalysisCache.h"
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

/// Dominant frequency via positive-going zero crossings over a probe
/// window - robust against the stretcher's amplitude ripple, sensitive to
/// exactly the thing the two modes differ in.
double measureFrequency(const juce::AudioBuffer<float>& buffer, double fromSeconds, double toSeconds)
{
    const auto from = static_cast<int>(fromSeconds * 44100.0);
    const auto to = static_cast<int>(toSeconds * 44100.0);
    REQUIRE(to > from);
    REQUIRE(to <= buffer.getNumSamples());

    auto crossings = 0;
    for (auto i = from + 1; i < to; ++i)
        if (buffer.getSample(0, i - 1) <= 0.0f && buffer.getSample(0, i) > 0.0f)
            ++crossings;

    return crossings / (toSeconds - fromSeconds);
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

SCENARIO("stretch mode keeps the pitch while the length changes", "[engine][clipspeed][stretch]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture(createSineAudioFile(440.0, 2.0));
        auto item = fixture.item();

        WHEN("a half-speed clip is bounced in both modes")
        {
            item->setSpeedRatio(0.5);
            fixture.commit();
            auto rePitched = fixture.bounce();

            item->setStretchMode(StretchMode::Stretch);
            fixture.commit();
            auto stretched = fixture.bounce();

            THEN("re-pitch drops an octave, stretch holds 440 Hz - at the same length")
            {
                REQUIRE(rePitched.getNumSamples() == Catch::Approx(4.0 * 44100.0).margin(512));
                REQUIRE(stretched.getNumSamples() == Catch::Approx(4.0 * 44100.0).margin(512));

                REQUIRE(measureFrequency(rePitched, 0.5, 3.5) == Catch::Approx(220.0).epsilon(0.02));
                REQUIRE(measureFrequency(stretched, 0.5, 3.5) == Catch::Approx(440.0).epsilon(0.02));
            }

            THEN("the stretched level survives within a phase-vocoder tolerance")
            {
                REQUIRE(stretched.getRMSLevel(0, static_cast<int>(0.5 * 44100.0),
                                              static_cast<int>(3.0 * 44100.0))
                        == Catch::Approx(1.0 / std::sqrt(2.0)).epsilon(0.1));
            }
        }

        WHEN("a double-speed stretch clip is bounced")
        {
            item->setSpeedRatio(2.0);
            item->setStretchMode(StretchMode::Stretch);
            fixture.commit();
            auto buffer = fixture.bounce();

            THEN("half the length, still 440 Hz")
            {
                REQUIRE(buffer.getNumSamples() == Catch::Approx(1.0 * 44100.0).margin(512));
                REQUIRE(measureFrequency(buffer, 0.1, 0.9) == Catch::Approx(440.0).epsilon(0.02));
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("a stretch-mode clip starts aligned with the timeline", "[engine][clipspeed][stretch]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        // one silent second, then the tone: the stretched onset must land at
        // its speed-scaled timeline position, which pins down the stretcher's
        // latency priming
        auto fixture = makeFixture(createSineAudioFile(440.0, 1.0, 1.0));
        auto item = fixture.item();

        item->setSpeedRatio(0.5);
        item->setStretchMode(StretchMode::Stretch);
        fixture.commit();
        auto buffer = fixture.bounce();

        THEN("the onset lands at 2.0 timeline seconds")
        {
            auto onset = -1;
            for (auto i = 0; i < buffer.getNumSamples(); ++i)
            {
                if (std::abs(buffer.getSample(0, i)) > 0.1f)
                {
                    onset = i;
                    break;
                }
            }

            REQUIRE(onset >= 0);
            REQUIRE(onset / 44100.0 == Catch::Approx(2.0).margin(0.05));
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
            REQUIRE_FALSE(plain.contains("stretch_mode"));
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

        WHEN("stretch mode is set and the item state round-trips")
        {
            item->setStretchMode(StretchMode::Stretch);

            json withMode;
            REQUIRE(item->writeToJson(withMode));
            REQUIRE(withMode.at("stretch_mode").get<int>() == 1);

            item->setStretchMode(StretchMode::RePitch);
            REQUIRE(item->readFromJson(withMode, false));

            THEN("the stored mode comes back, and a modeless state resets it")
            {
                REQUIRE(item->getStretchMode() == StretchMode::Stretch);

                REQUIRE(item->readFromJson(plain, false));
                REQUIRE(item->getStretchMode() == StretchMode::RePitch);
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
            fixture.item()->setStretchMode(StretchMode::Stretch);
            auto clone = playList->clonePlayListItem(fixture.item());
            REQUIRE(clone != nullptr);

            THEN("the clone keeps the speed and the mode")
            {
                REQUIRE(clone->getSpeedRatio() == Catch::Approx(0.5));
                REQUIRE(clone->getStretchMode() == StretchMode::Stretch);
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

SCENARIO("dropping a stretched clip keeps its speed and mode", "[engine][clipspeed]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture(createSlowSawTwoSecondsAudioFile());
        auto container = fixture.engine->getAudioTrackContainer();
        auto sourceTrack = container->getAudioTrack(0);
        auto item = fixture.item();

        item->setSpeedRatio(0.5);
        item->setStretchMode(StretchMode::Stretch);

        auto expectSpeedCopied = [] (const PlayListItem& copy)
        {
            REQUIRE(copy.getSpeedRatio() == Catch::Approx(0.5));
            REQUIRE(copy.getStretchMode() == StretchMode::Stretch);
        };

        WHEN("the clip is dragged onto a second track")
        {
            auto targetTrack = container->createNewAudioTrack(juce::String());
            REQUIRE(targetTrack != nullptr);

            targetTrack->dropPlayListItem(item, 96.0, audium::clocks);

            THEN("the moved clip keeps the speed and the mode")
            {
                REQUIRE(sourceTrack->getPlayListContainer()->getPlayListItems().empty());

                auto moved = targetTrack->getPlayListContainer()->getPlayListItems();
                REQUIRE(moved.size() == 1);
                expectSpeedCopied(*moved[0]);

                // and the timeline extent follows: 2 s source at half speed
                REQUIRE(moved[0]->getAbsolutePositionRange(audium::seconds).getLength()
                        == Catch::Approx(4.0));
            }
        }

        WHEN("the clip is dropped as a new item")
        {
            sourceTrack->dropPlayListItem(item, 96.0 * 8.0, audium::clocks, true);

            THEN("the copy keeps the speed and the mode")
            {
                auto items = sourceTrack->getPlayListContainer()->getPlayListItems();
                REQUIRE(items.size() == 2);
                expectSpeedCopied(*items[1]);
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

// ---------------------------------------------------------------------------
// Tempo lock: the clip's speed is project tempo / clip tempo, derived live.

SCENARIO("a tempo-locked clip derives its speed from the project tempo", "[engine][clipspeed][tempolock]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture(createSlowSawTwoSecondsAudioFile());
        auto item = fixture.item();
        auto tempoProvider = fixture.engine->getPlayListScheduler()->getTempoProvider();
        tempoProvider->setTempo(120.0);

        const auto extent = [&item] {
            return item->getAbsolutePositionRange(audium::seconds).getLength();
        };

        WHEN("the clip is locked at a clip tempo of 60")
        {
            item->setClipTempo(60.0);
            item->setTempoLocked(true);

            THEN("it plays at double speed on half the timeline, the source window untouched")
            {
                REQUIRE(item->isTempoLocked());
                REQUIRE(item->getSpeedRatio() == Catch::Approx(2.0));
                REQUIRE(extent() == Catch::Approx(1.0));
                REQUIRE(item->getRegionData(audium::seconds).getLength() == Catch::Approx(2.0));
            }

            THEN("a plain ratio is ignored while locked")
            {
                item->setSpeedRatio(0.5);
                REQUIRE(item->getSpeedRatio() == Catch::Approx(2.0));
            }

            THEN("a tempo change moves the ratio and the extent")
            {
                tempoProvider->setTempo(90.0);
                REQUIRE(item->getSpeedRatio() == Catch::Approx(1.5));
                REQUIRE(extent() == Catch::Approx(2.0 / 1.5));
            }

            THEN("unlocking bakes the derived ratio, so the clip stays put")
            {
                tempoProvider->setTempo(90.0);
                item->setTempoLocked(false);
                REQUIRE_FALSE(item->isTempoLocked());
                REQUIRE(item->getSpeedRatio() == Catch::Approx(1.5));

                tempoProvider->setTempo(120.0);
                REQUIRE(item->getSpeedRatio() == Catch::Approx(1.5));
            }

            THEN("the derived ratio is clamped to the speed range")
            {
                tempoProvider->setTempo(999.0);
                REQUIRE(item->getSpeedRatio() == Catch::Approx(ClipSpeed::maxSpeedRatio));
            }
        }

        WHEN("a clip is locked without a known tempo")
        {
            item->setTempoLocked(true);

            THEN("it plays as recorded until a tempo is set")
            {
                REQUIRE(item->getClipTempo() == 0.0);
                REQUIRE(item->getSpeedRatio() == Catch::Approx(1.0));

                item->setClipTempo(240.0);
                REQUIRE(item->getSpeedRatio() == Catch::Approx(0.5));
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("the tempo lock persists, resets and copies like the other item state", "[engine][clipspeed][tempolock]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture(createSlowSawTwoSecondsAudioFile());
        auto item = fixture.item();
        auto playList = fixture.engine->getAudioTrackContainer()->getAudioTrack(0)->getPlayListContainer();

        json plain;
        REQUIRE(item->writeToJson(plain));
        REQUIRE_FALSE(plain.contains("tempo_locked"));
        REQUIRE_FALSE(plain.contains("clip_tempo"));

        WHEN("a locked clip's state round-trips")
        {
            item->setClipTempo(128.0);
            item->setTempoLocked(true);

            json locked;
            REQUIRE(item->writeToJson(locked));
            REQUIRE(locked.at("tempo_locked").get<bool>());
            REQUIRE(locked.at("clip_tempo").get<double>() == Catch::Approx(128.0));

            item->setTempoLocked(false);
            item->setClipTempo(0.0);
            REQUIRE(item->readFromJson(locked, false));

            THEN("the lock and the tempo come back")
            {
                REQUIRE(item->isTempoLocked());
                REQUIRE(item->getClipTempo() == Catch::Approx(128.0));
            }

            THEN("restoring an unlocked state resets both - undo reuses items")
            {
                REQUIRE(item->readFromJson(plain, false));
                REQUIRE_FALSE(item->isTempoLocked());
                REQUIRE(item->getClipTempo() == 0.0);
            }
        }

        WHEN("a locked clip is cloned")
        {
            item->setClipTempo(128.0);
            item->setTempoLocked(true);
            auto clone = playList->clonePlayListItem(item);
            REQUIRE(clone != nullptr);

            THEN("the clone is locked at the same tempo")
            {
                REQUIRE(clone->isTempoLocked());
                REQUIRE(clone->getClipTempo() == Catch::Approx(128.0));
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("a tempo-locked clip bounces at the project tempo", "[engine][clipspeed][tempolock]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture(createSineAudioFile(440.0, 2.0));
        auto item = fixture.item();
        auto tempoProvider = fixture.engine->getPlayListScheduler()->getTempoProvider();

        item->setClipTempo(120.0);
        item->setTempoLocked(true);

        WHEN("the project runs at half the clip's tempo")
        {
            tempoProvider->setTempo(60.0);
            fixture.commit();
            auto rePitched = fixture.bounce();

            item->setStretchMode(StretchMode::Stretch);
            fixture.commit();
            auto stretched = fixture.bounce();

            THEN("both modes take twice the time; re-pitch drops an octave, stretch holds the pitch")
            {
                REQUIRE(rePitched.getNumSamples() == Catch::Approx(4.0 * 44100.0).margin(512));
                REQUIRE(stretched.getNumSamples() == Catch::Approx(4.0 * 44100.0).margin(512));
                REQUIRE(measureFrequency(rePitched, 0.5, 3.5) == Catch::Approx(220.0).epsilon(0.02));
                REQUIRE(measureFrequency(stretched, 0.5, 3.5) == Catch::Approx(440.0).epsilon(0.02));
            }
        }

        WHEN("the project runs at the clip's tempo")
        {
            tempoProvider->setTempo(120.0);
            fixture.commit();
            auto buffer = fixture.bounce();

            THEN("the clip plays as recorded")
            {
                REQUIRE(buffer.getNumSamples() == Catch::Approx(2.0 * 44100.0).margin(512));
                REQUIRE(measureFrequency(buffer, 0.2, 1.8) == Catch::Approx(440.0).epsilon(0.02));
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("a tempo-locked clip follows a tempo change while it plays", "[engine][clipspeed][tempolock]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        // 4 s of sine, locked at the project tempo: ratio 1.0. Two seconds
        // in, the tempo halves - the remaining two source seconds then take
        // four, so the clip ends at 6 s.
        auto fixture = makeFixture(createSineAudioFile(440.0, 4.0));
        auto item = fixture.item();
        auto tempoProvider = fixture.engine->getPlayListScheduler()->getTempoProvider();

        constexpr double bounceSeconds = 6.5;

        const auto bounceWithTempoDrop = [&] {
            tempoProvider->setTempo(120.0);
            item->setClipTempo(120.0);
            item->setTempoLocked(true);
            fixture.commit();

            auto config = std::make_shared<ExportAudioConfig>();
            config->fileName = fixture.bounceFile;
            config->sampleRate = 44100.0;
            config->blockSize = 512;
            config->numChannels = 1;
            config->positionSeconds = 0.0;
            config->lengthSeconds = bounceSeconds;

            auto dropped = false;
            auto playingAfterDrop = false;
            auto ratioAfterDrop = 0.0;

            AudioExporter(*fixture.engine, config).bounce([&] (double progress) {
                const auto seconds = progress * bounceSeconds;

                if (! dropped && seconds >= 2.0) {
                    tempoProvider->setTempo(60.0);
                    dropped = true;
                }

                // well inside the slow half: the voice must still be the
                // one started at 0 s, now at the new ratio
                if (dropped && seconds >= 4.0 && seconds < 4.1) {
                    const auto& voice = item->getVoiceSources().front();
                    playingAfterDrop = voice->isPlaying();
                    ratioAfterDrop = voice->getSpeedRatio();
                }
                return true;
            });

            REQUIRE(dropped);
            REQUIRE(playingAfterDrop);
            REQUIRE(ratioAfterDrop == Catch::Approx(0.5));

            return audioFileToAudioBuffer(fixture.bounceFile);
        };

        WHEN("a re-pitched clip is bounced across the tempo drop")
        {
            auto buffer = bounceWithTempoDrop();

            THEN("the pitch halves at the drop and the clip runs on to 6 s, no early stop")
            {
                REQUIRE(measureFrequency(buffer, 0.5, 1.8) == Catch::Approx(440.0).epsilon(0.02));
                REQUIRE(measureFrequency(buffer, 3.5, 5.5) == Catch::Approx(220.0).epsilon(0.02));

                const auto rms = [&buffer] (double from, double to) {
                    return buffer.getRMSLevel(0, static_cast<int>(from * 44100.0),
                                              static_cast<int>((to - from) * 44100.0));
                };
                REQUIRE(rms(5.5, 5.9) > 0.5);
                REQUIRE(rms(6.1, 6.4) < 0.01);
            }
        }

        WHEN("a time-stretched clip is bounced across the tempo drop")
        {
            item->setStretchMode(StretchMode::Stretch);
            auto buffer = bounceWithTempoDrop();

            THEN("the pitch holds on both sides of the drop")
            {
                REQUIRE(measureFrequency(buffer, 0.5, 1.8) == Catch::Approx(440.0).epsilon(0.02));
                REQUIRE(measureFrequency(buffer, 3.5, 5.5) == Catch::Approx(440.0).epsilon(0.02));
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("locking seeds the clip tempo from the beat analysis", "[engine][clipspeed][tempolock]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture(createSlowSawTwoSecondsAudioFile());
        auto item = fixture.item();
        auto provider = fixture.engine->getAudioTrackContainer()->getAnalysisProvider();
        REQUIRE(provider != nullptr);

        const auto file = ClipTempo::sourceFile(*item);
        REQUIRE(file.existsAsFile());

        WHEN("the source has no beat analysis")
        {
            const auto known = ClipTempo::lockToTempo(*item, *provider);

            THEN("the clip is locked, its tempo unknown")
            {
                REQUIRE_FALSE(known);
                REQUIRE(item->isTempoLocked());
                REQUIRE(item->getClipTempo() == 0.0);
                REQUIRE(item->getSpeedRatio() == Catch::Approx(1.0));
            }
        }

        WHEN("the analysis cache knows the source's tempo")
        {
            provider->getCache()->put(file, AnalysisType::BeatDegara, { 0.5f, 1.0f, 1.5f }, 96.0f);
            const auto known = ClipTempo::lockToTempo(*item, *provider);

            THEN("the clip takes it")
            {
                REQUIRE(known);
                REQUIRE(item->getClipTempo() == Catch::Approx(96.0));
            }

            THEN("a tempo the user set already is kept")
            {
                item->setClipTempo(192.0);
                ClipTempo::lockToTempo(*item, *provider);
                REQUIRE(item->getClipTempo() == Catch::Approx(192.0));
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
