#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Resource/AudioResourceContainer.h"

// File > Import...: AudioTrackContainer::importAudioFiles lays several files
// out in one of three ways and registers the whole import as one undo step.

using namespace audium;

namespace {

const auto importTestFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");

int numChannelsOf(AudiumEngine& engine, const File& file)
{
    std::unique_ptr<AudioFormatReader> reader(engine.getAudioResourceContainer()->getAudioFormatManager()->createReaderFor(file));
    return reader != nullptr ? (int) reader->numChannels : 0;
}

/** the tracks the import added, i.e. those after the baseline count */
std::vector<std::shared_ptr<AudioTrack>> tracksAfter(AudioTrackContainer& tracks, int baseline)
{
    std::vector<std::shared_ptr<AudioTrack>> result;
    for (auto i = baseline; i < tracks.getNumItems(); ++i)
        result.push_back(tracks.getAudioTrack(i));
    return result;
}

} // namespace

SCENARIO("importing audio files lays them out as chosen", "[engine][import]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    engine->getProjectSerializer()->createNewProject();

    auto first = File(importTestFilesDirectory + "120-funk-1-sec.wav");
    auto second = File(importTestFilesDirectory + "sine-0dB.wav");
    REQUIRE(first.existsAsFile());
    REQUIRE(second.existsAsFile());
    const StringArray files { first.getFullPathName(), second.getFullPathName() };

    auto tracks = engine->getAudioTrackContainer();
    const auto baseline = tracks->getNumItems();
    const auto position = 96000.0;

    std::string reported;
    auto callback = [&reported] (std::string failed) { reported = failed; };

    WHEN("two files are imported as separate tracks") {
        auto imported = tracks->importAudioFiles(files, position, AudioTrackContainer::ImportPlacement::separateTracks, callback);

        THEN("each file gets a track with one clip at the position") {
            REQUIRE(imported == 2);
            auto added = tracksAfter(*tracks, baseline);
            REQUIRE(added.size() == 2);
            for (auto& track : added) {
                auto items = track->getPlayListContainer()->getPlayListItems();
                REQUIRE(items.size() == 1);
                REQUIRE(items[0]->getAbsolutePosition(audium::clocks) == Catch::Approx(position));
            }
            REQUIRE(reported.empty());
        }
    }

    WHEN("two files are imported as stacked channels") {
        auto imported = tracks->importAudioFiles(files, position, AudioTrackContainer::ImportPlacement::stackedChannels, callback);

        THEN("one track holds one clip carrying the channels of both files") {
            REQUIRE(imported == 2);
            auto added = tracksAfter(*tracks, baseline);
            REQUIRE(added.size() == 1);
            REQUIRE(added[0]->getPlayListContainer()->getPlayListItems().size() == 1);
            REQUIRE(added[0]->getResourceGroups().size() == 1);
            REQUIRE(added[0]->getNumAudioTrackChannels() == numChannelsOf(*engine, first) + numChannelsOf(*engine, second));
        }
    }

    WHEN("two files are imported back to back") {
        auto imported = tracks->importAudioFiles(files, position, AudioTrackContainer::ImportPlacement::backToBack, callback);

        THEN("one track holds two clips, the second starting where the first ends") {
            REQUIRE(imported == 2);
            auto added = tracksAfter(*tracks, baseline);
            REQUIRE(added.size() == 1);
            auto items = added[0]->getPlayListContainer()->getPlayListItems();
            REQUIRE(items.size() == 2);
            REQUIRE(items[0]->getAbsolutePosition(audium::clocks) == Catch::Approx(position));
            REQUIRE(items[1]->getAbsolutePosition(audium::clocks)
                    == Catch::Approx(items[0]->getAbsolutePositionRange(audium::clocks).getEnd()));
        }
    }

    WHEN("one of the files cannot be read") {
        auto missing = File(importTestFilesDirectory + "does-not-exist.wav").getFullPathName();
        auto imported = tracks->importAudioFiles({ first.getFullPathName(), missing },
                                                 position, AudioTrackContainer::ImportPlacement::separateTracks, callback);

        THEN("the good file is imported and the bad one reported") {
            REQUIRE(imported == 1);
            REQUIRE(tracksAfter(*tracks, baseline).size() == 1);
            REQUIRE(String(reported).contains("does-not-exist.wav"));
        }
    }

    tracks = nullptr;
    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("importing audio files into an existing track", "[engine][import][track]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    engine->getProjectSerializer()->createNewProject();

    auto first = File(importTestFilesDirectory + "120-funk-1-sec.wav");
    auto second = File(importTestFilesDirectory + "sine-0dB.wav");
    const StringArray files { first.getFullPathName(), second.getFullPathName() };

    // a track that already holds a clip at 0
    auto tracks = engine->getAudioTrackContainer();
    REQUIRE(tracks->addAudioFiles({ first.getFullPathName() }, 0.0, nullptr, false));
    const auto numTracks = tracks->getNumItems();
    const auto targetIndex = numTracks - 1;
    auto target = tracks->getAudioTrack(targetIndex);
    // the undoable import rebuilds the tracks, so look the track up again afterwards
    auto targetNow = [&tracks, targetIndex] { return tracks->getAudioTrack(targetIndex); };
    const auto existingEnd = target->getPlayListContainer()->getPlayListItems()[0]->getAbsolutePositionRange(audium::clocks).getEnd();
    const auto channelsBefore = target->getNumAudioTrackChannels();
    const auto position = existingEnd * 2.0;   // clear of the existing clip

    using Placement = AudioTrackContainer::ImportPlacement;

    WHEN("two files are imported stacked") {
        REQUIRE(tracks->importAudioFiles(files, position, Placement::stackedChannels, nullptr, target) == 2);

        THEN("the track gets one new clip carrying both files and no track is added") {
            REQUIRE(tracks->getNumItems() == numTracks);
            auto items = targetNow()->getPlayListContainer()->getPlayListItems();
            REQUIRE(items.size() == 2);
            REQUIRE(items[1]->getAbsolutePosition(audium::clocks) == Catch::Approx(position));
            // clips share the track's channel lanes, each starting at the first one
            REQUIRE(targetNow()->getNumAudioTrackChannels()
                    == jmax(channelsBefore, numChannelsOf(*engine, first) + numChannelsOf(*engine, second)));
        }
    }

    WHEN("two files are imported back to back") {
        REQUIRE(tracks->importAudioFiles(files, position, Placement::backToBack, nullptr, target) == 2);

        THEN("the track gets two clips after its existing one, the second following the first") {
            REQUIRE(tracks->getNumItems() == numTracks);
            auto items = targetNow()->getPlayListContainer()->getPlayListItems();
            REQUIRE(items.size() == 3);
            REQUIRE(items[1]->getAbsolutePosition(audium::clocks) == Catch::Approx(position));
            REQUIRE(items[2]->getAbsolutePosition(audium::clocks)
                    == Catch::Approx(items[1]->getAbsolutePositionRange(audium::clocks).getEnd()));
        }
    }

    WHEN("separate tracks are asked for") {
        REQUIRE(tracks->importAudioFiles(files, position, Placement::separateTracks, nullptr, target) == 2);

        THEN("the files are stacked on the track instead") {
            REQUIRE(tracks->getNumItems() == numTracks);
            REQUIRE(targetNow()->getPlayListContainer()->getPlayListItems().size() == 2);
        }
    }

    WHEN("nothing can be read") {
        REQUIRE(tracks->importAudioFiles({ importTestFilesDirectory + "does-not-exist.wav" }, position,
                                         Placement::stackedChannels, nullptr, target) == 0);

        THEN("the target track is kept") {
            REQUIRE(tracks->getNumItems() == numTracks);
            REQUIRE(targetNow()->getPlayListContainer()->getPlayListItems().size() == 1);
        }
    }

    target = nullptr;
    tracks = nullptr;
    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("undoing an import removes every track it added", "[engine][import][undo]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    engine->getProjectSerializer()->createNewProject();

    auto first = File(importTestFilesDirectory + "120-funk-1-sec.wav");
    auto second = File(importTestFilesDirectory + "sine-0dB.wav");
    const StringArray files { first.getFullPathName(), second.getFullPathName() };

    // the container is rebuilt by undo, so re-query it instead of holding it
    auto numTracks = [&engine] { return engine->getAudioTrackContainer()->getNumItems(); };
    const auto baseline = numTracks();
    engine->getUndoManager()->beginNewTransaction();

    WHEN("an import is undone, for each placement") {
        using Placement = AudioTrackContainer::ImportPlacement;
        for (auto placement : { Placement::separateTracks, Placement::stackedChannels, Placement::backToBack }) {
            REQUIRE(engine->getAudioTrackContainer()->importAudioFiles(files, 0.0, placement, nullptr) == 2);
            REQUIRE(numTracks() > baseline);
            engine->getUndoManager()->undo();

            // one step takes the whole import back
            REQUIRE(numTracks() == baseline);
        }
    }

    WHEN("a single-track add with undo is undone") {
        REQUIRE(engine->getAudioTrackContainer()->addAudioFiles({ first.getFullPathName() }, 0.0, nullptr, true));
        REQUIRE(numTracks() == baseline + 1);
        engine->getUndoManager()->undo();

        THEN("no empty track is left behind") {
            REQUIRE(numTracks() == baseline);
        }
    }

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
