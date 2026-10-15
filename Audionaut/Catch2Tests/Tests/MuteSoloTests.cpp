//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Selection/SelectionManager.h"

#include "LiveRenderHarness.h"
#include "TestEngine.h"
#include "TestUtils.h"

// Mute and solo live twice: on the track's channels (the model) and in the
// bus renderer, indexed by bus channel = track channel offset + channel.
// Removing a track shifts every later track down the bus, so the renderer's
// per-index state must move with the tracks - a soloed track that is gone
// must not keep silencing (or soloing) whatever now sits at its index.

using namespace audium;
using namespace audium::test;

namespace {

constexpr double kSampleRate = 44100.0;
constexpr int kBlockSize = 512;
constexpr float kAudible = 0.1f;    // the clips are full-scale sines
constexpr float kSilent = 1.0e-4f;

/// Plays from the start through the live callback and returns each bus
/// channel's post-fader peak in the last block rendered.
std::vector<float> busLevelsWhilePlaying (AudiumEngine& engine, LiveBlockDriver& driver)
{
    driver.play (0.0);
    driver.render (0.5);   // well past the 10 ms gain ramps, inside the 2 s clips
    REQUIRE (driver.lastIsPlaying());

    auto bus = engine.getAudioBusInterface();
    std::vector<float> levels;
    for (auto c = 0; c < bus->getNumAudioBusChannels(); ++c)
        levels.push_back (bus->getChannelLevel (c));

    driver.stop();
    return levels;
}

/// Every bus channel carries the mixer state of the track that owns it now.
void requireBusFollowsTracks (AudiumEngine& engine)
{
    auto tracks = engine.getAudioTrackContainer();
    auto bus = engine.getAudioBusInterface();
    REQUIRE (bus->getNumAudioBusChannels() == tracks->getNumAudioTrackChannels());

    for (auto t = 0; t < tracks->getNumItems(); ++t)
    {
        auto track = tracks->getAudioTrack (t);
        for (auto c = 0; c < track->getNumAudioTrackChannels(); ++c)
        {
            const auto busChannel = track->getChannelOffset() + c;
            const auto data = bus->getChannelData (busChannel);
            CAPTURE (t, c, busChannel);
            REQUIRE (data.trackId == track->getId());
            REQUIRE (data.solo == track->getSolo (c));
            REQUIRE (data.mute == track->getMute (c));
        }
    }
}

/// A mixer button press as the channel strip records it: one undo step.
void press (AudioTrack& track, const juce::String& name, std::function<void()> change)
{
    track.onDragStart (0);
    change();
    track.onDragEnd (name);
}

/// Three mono tracks, each playing the same full-scale sine from 0.
void createThreeTracks (TestEngine& engine)
{
    const auto sine = createSineAudioFile (440.0, 2.0);
    for (auto i = 0; i < 3; ++i)
        REQUIRE (engine.open (sine));

    REQUIRE (engine.tracks()->getNumItems() == 3);
    for (auto i = 0; i < 3; ++i)
        REQUIRE (engine.track (i)->getNumAudioTrackChannels() == 1);
}

/// Removes the track the way the track list does: select it, delete the
/// selection (one undo step).
void deleteAsInTrackList (TestEngine& engine, std::shared_ptr<AudioTrack> track)
{
    auto selection = engine.tracks()->getSelectionManager();
    selection->clear();
    selection->selectItem (track, true);
    engine.tracks()->deleteSelectedObjects();
}

} // namespace

SCENARIO ("removing a soloed track hands the bus back to the remaining tracks",
          "[engine][integration][solo]")
{
    GIVEN ("three tracks playing the same clip, track 1 soloed")
    {
        TestEngine engine;
        createThreeTracks (engine);
        auto tracks = engine.tracks();
        auto track1 = engine.track (0);
        auto track2 = engine.track (1);
        auto track3 = engine.track (2);

        // the device runs throughout, as in the app: the button is pressed
        // on a bus that has rendered with all three tracks
        FakeAudioIODevice device (kSampleRate, kBlockSize);
        LiveBlockDriver driver (*engine, device);
        driver.idle (1);

        press (*track1, "Solo", [&] { track1->setSolo (true, 0); });
        REQUIRE (tracks->anyChannelSolo());

        THEN ("only track 1 is heard")
        {
            const auto levels = busLevelsWhilePlaying (*engine, driver);
            REQUIRE (levels.size() == 3);
            CHECK (levels[0] > kAudible);
            CHECK (levels[1] < kSilent);
            CHECK (levels[2] < kSilent);
            requireBusFollowsTracks (*engine);
        }

        WHEN ("track 1 is selected and deleted, as in the track list")
        {
            deleteAsInTrackList (engine, track1);
            track1 = nullptr;

            THEN ("nothing is soloed any more and tracks 2 and 3 are both heard")
            {
                REQUIRE (tracks->getNumItems() == 2);
                REQUIRE (engine.track (0) == track2);
                REQUIRE (engine.track (1) == track3);
                REQUIRE_FALSE (tracks->anyChannelSolo());

                const auto levels = busLevelsWhilePlaying (*engine, driver);
                REQUIRE (levels.size() == 2);
                CHECK (levels[0] > kAudible);   // track 2, now on bus channel 0
                CHECK (levels[1] > kAudible);   // track 3, now on bus channel 1
                requireBusFollowsTracks (*engine);
            }

            THEN ("undo brings the soloed track 1 back and it is the only one heard again")
            {
                tracks->getUndoManager()->undo();
                REQUIRE (tracks->getNumItems() == 3);
                REQUIRE (engine.track (0)->getSolo (0));

                const auto levels = busLevelsWhilePlaying (*engine, driver);
                REQUIRE (levels.size() == 3);
                CHECK (levels[0] > kAudible);
                CHECK (levels[1] < kSilent);
                CHECK (levels[2] < kSilent);
                requireBusFollowsTracks (*engine);
            }
        }

        WHEN ("track 1 is deleted directly, as the remove-track verb and stem separation do")
        {
            REQUIRE (tracks->deleteAudioTrack (track1));
            track1 = nullptr;

            THEN ("nothing is soloed any more and tracks 2 and 3 are both heard")
            {
                REQUIRE (tracks->getNumItems() == 2);
                REQUIRE_FALSE (tracks->anyChannelSolo());

                const auto levels = busLevelsWhilePlaying (*engine, driver);
                REQUIRE (levels.size() == 2);
                CHECK (levels[0] > kAudible);   // track 2, now on bus channel 0
                CHECK (levels[1] > kAudible);   // track 3, now on bus channel 1
                requireBusFollowsTracks (*engine);
            }
        }
    }
}

SCENARIO ("removing a track keeps a later track's mute with that track",
          "[engine][integration][mute]")
{
    GIVEN ("three tracks playing the same clip, track 3 muted")
    {
        TestEngine engine;
        createThreeTracks (engine);
        auto track3 = engine.track (2);

        FakeAudioIODevice device (kSampleRate, kBlockSize);
        LiveBlockDriver driver (*engine, device);
        driver.idle (1);

        press (*track3, "Mute", [&] { track3->setMute (true, 0); });

        WHEN ("track 1 is selected and deleted, as in the track list")
        {
            deleteAsInTrackList (engine, engine.track (0));

            THEN ("track 2 is heard and track 3 stays silent on its new bus channel")
            {
                REQUIRE (engine.track (1) == track3);
                REQUIRE (track3->getMute (0));

                const auto levels = busLevelsWhilePlaying (*engine, driver);
                REQUIRE (levels.size() == 2);
                CHECK (levels[0] > kAudible);   // track 2
                CHECK (levels[1] < kSilent);    // track 3, muted
                requireBusFollowsTracks (*engine);
            }
        }

        WHEN ("a channel is added to track 1, moving tracks 2 and 3 up the bus")
        {
            REQUIRE (engine.track (0)->addChannel() != nullptr);

            THEN ("track 2 is heard and track 3 stays silent on its new bus channel")
            {
                const auto levels = busLevelsWhilePlaying (*engine, driver);
                REQUIRE (levels.size() == 4);
                CHECK (levels[0] > kAudible);   // track 1
                CHECK (levels[1] < kSilent);    // track 1's new channel, nothing on it
                CHECK (levels[2] > kAudible);   // track 2
                CHECK (levels[3] < kSilent);    // track 3, muted
                requireBusFollowsTracks (*engine);
            }
        }
    }
}

SCENARIO ("a new project starts without the previous project's solo",
          "[engine][integration][solo]")
{
    GIVEN ("a project with track 1 soloed")
    {
        TestEngine engine;
        createThreeTracks (engine);

        FakeAudioIODevice device (kSampleRate, kBlockSize);
        LiveBlockDriver driver (*engine, device);
        driver.idle (1);

        press (*engine.track (0), "Solo", [&] { engine.track (0)->setSolo (true, 0); });

        WHEN ("a new project replaces it")
        {
            engine->getProjectSerializer()->cleanup();
            engine->getProjectSerializer()->createNewProject (3);
            driver.idle (1);

            THEN ("no bus channel is left soloed")
            {
                REQUIRE_FALSE (engine.tracks()->anyChannelSolo());
                auto bus = engine->getAudioBusInterface();
                for (auto c = 0; c < bus->getNumAudioBusChannels(); ++c)
                {
                    CAPTURE (c);
                    REQUIRE_FALSE (bus->getChannelData (c).solo);
                }
                requireBusFollowsTracks (*engine);
            }
        }
    }
}

SCENARIO ("a project saved with a soloed track opens with only that track heard",
          "[engine][integration][solo][mute]")
{
    const auto package = File (String (CURRENT_SOURCE_DIR) + "/TestFiles/Sessions/solo-reopen-test.audium");
    const auto projectFile = package.getChildFile (ProjectFileStore::projectFileName);

    GIVEN ("three mono tracks, track 1 soloed and track 3 muted, saved")
    {
        {
            TestEngine engine;
            createThreeTracks (engine);

            press (*engine.track (0), "Solo", [&] { engine.track (0)->setSolo (true, 0); });
            press (*engine.track (2), "Mute", [&] { engine.track (2)->setMute (true, 0); });
            REQUIRE (engine->getProjectFileStore()->save (projectFile, nullptr));
        }

        WHEN ("it is opened in a fresh session with the device running")
        {
            TestEngine engine;
            FakeAudioIODevice device (kSampleRate, kBlockSize);
            LiveBlockDriver driver (*engine, device);
            driver.idle (1);

            REQUIRE (engine.open (package));
            REQUIRE (engine.tracks()->getNumItems() == 3);
            REQUIRE (engine.track (0)->getSolo (0));
            REQUIRE (engine.track (2)->getMute (0));

            THEN ("only track 1 is heard")
            {
                const auto levels = busLevelsWhilePlaying (*engine, driver);
                REQUIRE (levels.size() == 3);
                CHECK (levels[0] > kAudible);
                CHECK (levels[1] < kSilent);
                CHECK (levels[2] < kSilent);
                requireBusFollowsTracks (*engine);
            }
        }
    }

    // cleanup ... comment out in case you need to isolate an issue
    package.deleteRecursively();
}
