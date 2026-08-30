//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/ResourceGroup.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/Separation/FakeSeparationBackend.h"
#include "Engine/Separation/StemSeparator.h"

#include "TestUtils.h"

// The separator's plumbing - render, backend, import, undo - exercised with
// the fake backend, which needs no model and returns the input as the
// Vocals stem.

using namespace audium;

namespace {

struct Fixture {
    std::shared_ptr<AudiumEngine> engine;
    std::shared_ptr<FakeSeparationBackend> backend;

    // Every accessor re-queries: an undoable edit rebuilds the container.
    std::shared_ptr<AudioTrackContainer> container() const { return engine->getAudioTrackContainer(); }

    size_t trackCount() const { return container()->getAudioTracks().size(); }

    std::shared_ptr<PlayListItem> clip (int trackId, int index = 0) const
    {
        auto track = container()->getAudioTrack (trackId);
        return track != nullptr ? track->getPlayListContainer()->getPlayListItem (index) : nullptr;
    }

    std::shared_ptr<AudioResource> resource (int trackId) const
    {
        auto track = container()->getAudioTrack (trackId);

        if (track == nullptr || track->getResourceGroups().empty()
            || track->getResourceGroups()[0]->getAudioResources().empty())
            return nullptr;

        return track->getResourceGroups()[0]->getAudioResources()[0];
    }

    ~Fixture() { engine = nullptr; }
};

Fixture makeFixture (const juce::File& audioFile, double positionClocks = 0.0)
{
    Fixture fixture;
    fixture.engine = AudiumFactory::createAudiumEngine();
    fixture.backend = std::make_shared<FakeSeparationBackend>();

    REQUIRE (audioFile.existsAsFile());
    REQUIRE (fixture.container()->addAudioFiles ({ audioFile.getFullPathName() }, positionClocks, nullptr, false));
    REQUIRE (fixture.trackCount() == 1);
    REQUIRE (fixture.clip (0) != nullptr);

    return fixture;
}

float rms (const juce::AudioBuffer<float>& buffer)
{
    double sum = 0.0;
    auto count = 0;

    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (auto i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto sample = buffer.getSample (channel, i);
            sum += sample * sample;
            ++count;
        }

    return count > 0 ? static_cast<float> (std::sqrt (sum / count)) : 0.0f;
}

} // namespace

SCENARIO ("stem separation turns a clip into four stem tracks", "[engine][separation]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock (Thread::getCurrentThread());

    {
        // A 2-second mono saw placed at bar 3, so alignment is not trivially zero.
        const auto positionClocks = 2.0 * 96.0;
        auto fixture = makeFixture (createSlowSawTwoSecondsAudioFile(), positionClocks);
        const auto sourceLength = fixture.clip (0)->getRegion()->getRegionData (audium::seconds).getLength();
        const auto sourceRms = rms (audioFileToAudioBuffer (juce::File (fixture.resource (0)->getFullPathName())));
        REQUIRE (sourceRms > 0.01f);

        StemSeparator separator (fixture.engine, fixture.backend);

        SeparationConfig config;
        config.trackId = 0;
        config.numThreads = 2;

        juce::String reason;
        REQUIRE (separator.canSeparate (config, reason));

        WHEN ("the clip is separated")
        {
            std::vector<int> newTrackIds;
            juce::String error;
            std::vector<double> reported;

            const auto ok = separator.separate (config,
                                                [&reported] (double fraction, const juce::String&)
                                                {
                                                    reported.push_back (fraction);
                                                    return true;
                                                },
                                                newTrackIds, error);

            THEN ("it succeeds and reports monotonic progress up to 1")
            {
                INFO ("error: " << error);
                REQUIRE (ok);
                REQUIRE (error.isEmpty());
                REQUIRE_FALSE (reported.empty());
                REQUIRE (std::is_sorted (reported.begin(), reported.end()));
                REQUIRE (reported.back() == Catch::Approx (1.0));
            }

            THEN ("four tracks are added after the source, named after the clip and stem")
            {
                REQUIRE (fixture.trackCount() == 5);
                REQUIRE (newTrackIds == std::vector<int> { 1, 2, 3, 4 });

                const auto clipName = fixture.clip (0)->getRegion()->getName();
                REQUIRE (fixture.container()->getAudioTrack (1)->getAudioTrackName() == clipName + " - Drums");
                REQUIRE (fixture.container()->getAudioTrack (2)->getAudioTrackName() == clipName + " - Bass");
                REQUIRE (fixture.container()->getAudioTrack (3)->getAudioTrackName() == clipName + " - Other");
                REQUIRE (fixture.container()->getAudioTrack (4)->getAudioTrackName() == clipName + " - Vocals");
            }

            THEN ("every stem clip starts where the source clip starts and is as long")
            {
                for (auto trackId = 1; trackId <= 4; ++trackId)
                {
                    auto stemClip = fixture.clip (trackId);
                    REQUIRE (stemClip != nullptr);
                    REQUIRE (stemClip->getAbsolutePosition (audium::clocks) == Catch::Approx (positionClocks));
                    REQUIRE (stemClip->getRegion()->getRegionData (audium::seconds).getLength()
                             == Catch::Approx (sourceLength).margin (0.01));
                }
            }

            THEN ("the stems are stereo files at the backend's rate")
            {
                for (auto trackId = 1; trackId <= 4; ++trackId)
                {
                    REQUIRE (fixture.container()->getAudioTrack (trackId)->getNumAudioTrackChannels() == 2);
                    REQUIRE (fixture.resource (trackId)->getSampleRate() == Catch::Approx (44100.0));
                }
            }

            THEN ("the Vocals stem carries the clip's audio and the others are silent")
            {
                auto vocals = audioFileToAudioBuffer (juce::File (fixture.resource (4)->getFullPathName()));
                REQUIRE (rms (vocals) == Catch::Approx (sourceRms).margin (0.02));

                for (auto trackId = 1; trackId <= 3; ++trackId)
                {
                    auto stem = audioFileToAudioBuffer (juce::File (fixture.resource (trackId)->getFullPathName()));
                    REQUIRE (rms (stem) == Catch::Approx (0.0).margin (1.0e-6));
                }
            }

            THEN ("the backend was handed two identical channels for the mono clip")
            {
                REQUIRE (fixture.backend->calls == 1);
                REQUIRE (fixture.backend->lastInput.getNumChannels() == 2);

                const auto& input = fixture.backend->lastInput;
                for (auto i = 0; i < input.getNumSamples(); i += 997)
                    REQUIRE (input.getSample (0, i) == input.getSample (1, i));
            }

            THEN ("one undo removes all four tracks and redo brings them back")
            {
                auto undoManager = fixture.container()->getUndoManager();
                REQUIRE (undoManager->canUndo());

                undoManager->undo();
                REQUIRE (fixture.trackCount() == 1);

                undoManager->redo();
                REQUIRE (fixture.trackCount() == 5);
            }

            THEN ("nothing is left in the scratch directory")
            {
                const auto scratch = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                         .getChildFile ("Audionaut").getChildFile ("Separation");
                REQUIRE (scratch.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0);
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO ("stem separation places stems earlier when the clip's fade-in extends ahead of it",
          "[engine][separation]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock (Thread::getCurrentThread());

    {
        const auto positionClocks = 4.0 * 96.0;
        auto fixture = makeFixture (createSlowSawTwoSecondsAudioFile(), positionClocks);

        // A fade-in starting a quarter of the clip before its start: the
        // render begins there, so the stems must too.
        auto clip = fixture.clip (0);
        clip->getDynamics().setFadeInStart (-0.25);
        const auto headSeconds = -clip->getDynamics().getFadeInStart (audium::seconds);
        REQUIRE (headSeconds > 0.0);
        const auto headClocks = fixture.container()->getTempoProvider()->secondsToClocks (headSeconds);

        StemSeparator separator (fixture.engine, fixture.backend);
        SeparationConfig config;
        config.trackId = 0;

        std::vector<int> newTrackIds;
        juce::String error;
        REQUIRE (separator.separate (config, nullptr, newTrackIds, error));

        THEN ("the stems start headExtension earlier than the source clip")
        {
            auto stemClip = fixture.clip (1);
            REQUIRE (stemClip != nullptr);
            REQUIRE (stemClip->getAbsolutePosition (audium::clocks)
                     == Catch::Approx (positionClocks - headClocks).margin (0.01));
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO ("stem separation leaves the project alone when cancelled or failing",
          "[engine][separation]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock (Thread::getCurrentThread());

    {
        auto fixture = makeFixture (createSlowSawTwoSecondsAudioFile());
        StemSeparator separator (fixture.engine, fixture.backend);
        SeparationConfig config;
        config.trackId = 0;

        const auto scratch = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("Audionaut").getChildFile ("Separation");

        WHEN ("the progress callback cancels")
        {
            std::vector<int> newTrackIds;
            juce::String error;

            const auto ok = separator.separate (config,
                                                [] (double fraction, const juce::String&) { return fraction < 0.5; },
                                                newTrackIds, error);

            THEN ("it stops without an error, adds no track and cleans up")
            {
                REQUIRE_FALSE (ok);
                REQUIRE (error.isEmpty());
                REQUIRE (newTrackIds.empty());
                REQUIRE (fixture.trackCount() == 1);
                REQUIRE_FALSE (fixture.container()->getUndoManager()->canUndo());
                REQUIRE (scratch.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0);
            }
        }

        WHEN ("the backend fails")
        {
            fixture.backend->failWith = "model exploded";

            std::vector<int> newTrackIds;
            juce::String error;
            const auto ok = separator.separate (config, nullptr, newTrackIds, error);

            THEN ("the failure is reported and nothing changes")
            {
                REQUIRE_FALSE (ok);
                REQUIRE (error == "model exploded");
                REQUIRE (fixture.trackCount() == 1);
                REQUIRE (scratch.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0);
            }
        }

        WHEN ("the backend is not ready")
        {
            fixture.backend->ready = false;

            juce::String reason;

            THEN ("canSeparate says so")
            {
                REQUIRE_FALSE (separator.canSeparate (config, reason));
                REQUIRE (reason == "fake backend not ready");
            }
        }

        WHEN ("the track does not exist")
        {
            config.trackId = 7;
            juce::String reason;

            THEN ("canSeparate says so")
            {
                REQUIRE_FALSE (separator.canSeparate (config, reason));
                REQUIRE (reason == "no such track");
            }
        }

        WHEN ("the clip is longer than the configured maximum")
        {
            config.maxClipSeconds = 1.0;
            juce::String reason;

            THEN ("canSeparate refuses")
            {
                REQUIRE_FALSE (separator.canSeparate (config, reason));
                REQUIRE (reason.contains ("longer"));
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO ("stem separation refuses tracks with more than two channels", "[engine][separation]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock (Thread::getCurrentThread());

    {
        auto fixture = makeFixture (createSlowSawTwoSecondsAudioFile());

        // Add the file twice more to the same clip position: the track grows
        // to three channels.
        const auto file = createSlowSawTwoSecondsAudioFile().getFullPathName();
        auto track = fixture.container()->getAudioTrack (0);
        REQUIRE (track->addAudioFiles ({ file, file }, 0.0, nullptr, false));
        REQUIRE (track->getNumAudioTrackChannels() == 3);

        StemSeparator separator (fixture.engine, fixture.backend);
        SeparationConfig config;
        config.trackId = 0;

        juce::String reason;
        REQUIRE_FALSE (separator.canSeparate (config, reason));
        REQUIRE (reason.contains ("mono or stereo"));
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO ("stem separation targets the selected clip", "[engine][separation]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock (Thread::getCurrentThread());

    {
        auto fixture = makeFixture (createSlowSawTwoSecondsAudioFile());
        StemSeparator separator (fixture.engine, fixture.backend);

        SeparationConfig config;

        WHEN ("nothing is selected")
        {
            REQUIRE_FALSE (separator.targetSelectedClip (config));
            REQUIRE (config.trackId == -1);
        }

        WHEN ("a clip is selected")
        {
            fixture.container()->getSelectionManager()->selectItem (fixture.clip (0), true);

            REQUIRE (separator.targetSelectedClip (config));
            REQUIRE (config.trackId == 0);
            REQUIRE (config.playlistItemId == 0);
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
