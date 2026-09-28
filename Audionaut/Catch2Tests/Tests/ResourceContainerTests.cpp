#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/ResourceGroup.h"

using namespace audium;

// The resource container is the registry behind every imported audio file:
// it copies the file into the project, hands out one resource per file
// channel and remembers which track and resource group each one belongs to.

static const auto resourceTestFile = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/120-funk-1-sec.wav"));

SCENARIO("the resource container tracks which track owns each audio resource", "[engine][resource][container]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto resources = engine->getAudioResourceContainer();
    auto tracks = engine->getAudioTrackContainer();

    const auto inFile = resourceTestFile;
    REQUIRE(inFile.existsAsFile());

    GIVEN("a new project with one empty track")
    {
        // a fresh project imports into the temp directory rather than into
        // whatever project directory an earlier test left behind
        engine->getProjectSerializer()->createNewProject();
        auto track = tracks->getAudioTrack(0);
        REQUIRE(track != nullptr);
        REQUIRE(resources->getNumAudioResources() == 0);
        REQUIRE(resources->getAudioTracks().empty());

        WHEN("a stereo file is added to the track")
        {
            REQUIRE(track->addAudioFiles({ inFile.getFullPathName() }, 0.0, nullptr, false));
            auto owned = resources->getAudioResourcesForTrack(track.get());

            THEN("one resource per channel is registered, all owned by that track")
            {
                REQUIRE(resources->getNumAudioResources() == 2);
                REQUIRE(owned.size() == 2);
                REQUIRE(resources->getAudioTracks() == std::vector<std::shared_ptr<AudioTrack>>{ track });
                for (auto& resource : owned) {
                    REQUIRE(resources->getAudioTrackForResource(resource) == track);
                    REQUIRE(resource->getAudioTrack() == track);
                }
                REQUIRE(owned[0]->getChannelMapping().getSourceChannel() == 0);
                REQUIRE(owned[1]->getChannelMapping().getSourceChannel() == 1);
                REQUIRE(track->getAudioResources() == owned);
            }

            THEN("the file was copied into the project's audio directory and is found by its url")
            {
                const auto copied = owned[0]->getLocalFile();
                REQUIRE(copied.existsAsFile());
                REQUIRE(copied != inFile);
                REQUIRE(copied.isAChildOf(resources->getCurrentAudioFileDirectory()));
                REQUIRE(copied.hasIdenticalContentTo(inFile));
                REQUIRE(owned[1]->getLocalFile() == copied);

                REQUIRE(resources->isAudioFileCurrentlyLoaded(copied));
                REQUIRE_FALSE(resources->isAudioFileCurrentlyLoaded(inFile));
                REQUIRE(resources->findResourceWithUrl(owned[0]->getUrl()) == owned[0]);
                REQUIRE(resources->findResourceWithUrl(juce::URL(inFile)) == nullptr);
            }

            THEN("both resources belong to the track's one resource group")
            {
                auto groups = track->getResourceGroups();
                REQUIRE(groups.size() == 1);
                REQUIRE(resources->getAudioResourcesForResourceGroup(groups[0].get()) == owned);
                REQUIRE(groups[0]->getAudioResources() == owned);
                REQUIRE(owned[0]->getResourceGroup() == groups[0]);
            }

            AND_WHEN("the same file is added to a second track")
            {
                auto second = tracks->createNewAudioTrack("Track 2");
                REQUIRE(second->addAudioFiles({ inFile.getFullPathName() }, 0.0, nullptr, false));
                auto ownedBySecond = resources->getAudioResourcesForTrack(second.get());

                THEN("each track owns its own pair and the audio file is not copied twice")
                {
                    REQUIRE(resources->getNumAudioResources() == 4);
                    REQUIRE(resources->getAudioResourcesForTrack(track.get()) == owned);
                    REQUIRE(ownedBySecond.size() == 2);
                    REQUIRE(resources->getAudioTracks() == std::vector<std::shared_ptr<AudioTrack>>{ track, second });
                    REQUIRE(ownedBySecond[0]->getLocalFile() == owned[0]->getLocalFile());

                    auto audioDir = resources->getCurrentAudioFileDirectory();
                    auto copies = audioDir.findChildFiles(File::findFiles, false, inFile.getFileNameWithoutExtension() + "*");
                    REQUIRE(copies.size() == 1);
                }

                AND_WHEN("one of the second track's resources is removed")
                {
                    auto removed = ownedBySecond[0];
                    resources->removeAudioResource(removed);

                    THEN("only that resource is gone")
                    {
                        REQUIRE(resources->getNumAudioResources() == 3);
                        REQUIRE(resources->getAudioResourcesForTrack(track.get()) == owned);
                        REQUIRE(resources->getAudioResourcesForTrack(second.get())
                                == std::vector<std::shared_ptr<AudioResource>>{ ownedBySecond[1] });
                        REQUIRE(resources->getAudioTrackForResource(removed) == nullptr);
                        // the other resources still reference the file
                        REQUIRE(resources->isAudioFileCurrentlyLoaded(removed->getLocalFile()));
                        REQUIRE(resources->findResourceWithUrl(removed->getUrl()) != nullptr);
                    }
                }

                AND_WHEN("all of the first track's resources are removed")
                {
                    resources->removeAudioResourcesForTrack(track.get());

                    THEN("the first track drops out of the container and the second one is untouched")
                    {
                        REQUIRE(resources->getNumAudioResources() == 2);
                        REQUIRE(resources->getAudioResourcesForTrack(track.get()).empty());
                        REQUIRE(track->getAudioResources().empty());
                        REQUIRE(resources->getAudioResourcesForTrack(second.get()) == ownedBySecond);
                        REQUIRE(resources->getAudioTracks() == std::vector<std::shared_ptr<AudioTrack>>{ second });
                    }
                }

                AND_WHEN("the second track is deleted from the project")
                {
                    REQUIRE(tracks->deleteAudioTrack(second));

                    THEN("its resources leave the container with it")
                    {
                        REQUIRE(tracks->getNumItems() == 1);
                        REQUIRE(resources->getNumAudioResources() == 2);
                        REQUIRE(resources->getAudioResourcesForTrack(second.get()).empty());
                        REQUIRE(resources->getAudioTracks() == std::vector<std::shared_ptr<AudioTrack>>{ track });
                    }
                }
            }

            AND_WHEN("the container is cleaned up")
            {
                resources->cleanup();

                THEN("nothing is registered anymore")
                {
                    REQUIRE(resources->getNumAudioResources() == 0);
                    REQUIRE(resources->getAudioResourcesForTrack(track.get()).empty());
                    REQUIRE(resources->getAudioTracks().empty());
                    REQUIRE(resources->findResourceWithUrl(owned[0]->getUrl()) == nullptr);
                }
            }
        }
    }

    tracks = nullptr;
    resources = nullptr;
    engine = nullptr;
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO("the resource container keeps resources apart by resource group", "[engine][resource][container]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto resources = engine->getAudioResourceContainer();

    const auto inFile = resourceTestFile;
    REQUIRE(inFile.existsAsFile());

    GIVEN("a track with two resource groups")
    {
        engine->getProjectSerializer()->createNewProject();
        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        REQUIRE(track != nullptr);
        auto groupA = track->createNewResourceGroup();
        auto groupB = track->createNewResourceGroup();
        REQUIRE(track->getResourceGroups().size() == 2);

        WHEN("a resource is added to the second group by url")
        {
            auto resource = groupB->addAudioResourceFromUrl(juce::URL(inFile));

            THEN("it is registered under that group only, but under the track as a whole")
            {
                REQUIRE(resource != nullptr);
                REQUIRE(resources->getNumAudioResources() == 1);
                REQUIRE(resource->getResourceGroup() == groupB);
                REQUIRE(resources->getAudioResourcesForResourceGroup(groupB.get())
                        == std::vector<std::shared_ptr<AudioResource>>{ resource });
                REQUIRE(resources->getAudioResourcesForResourceGroup(groupA.get()).empty());
                REQUIRE(groupA->getAudioResources().empty());
                REQUIRE(resources->getAudioResourcesForTrack(track.get())
                        == std::vector<std::shared_ptr<AudioResource>>{ resource });
                REQUIRE(resources->getAudioTrackForResource(resource) == track);
            }

            AND_WHEN("the group's resources are cleaned up")
            {
                groupB->cleanupAudioResources();

                THEN("the container has let go of it")
                {
                    REQUIRE(resources->getNumAudioResources() == 0);
                    REQUIRE(resources->getAudioResourcesForResourceGroup(groupB.get()).empty());
                    REQUIRE(resources->getAudioTrackForResource(resource) == nullptr);
                }
            }
        }
    }

    resources = nullptr;
    engine = nullptr;
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}
