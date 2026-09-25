#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Resource/AudioResource.h"

#if !JUCE_WINDOWS
 #include <sys/stat.h>
#endif

using namespace audium;

static const auto saveFailureTestFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");

// every loaded audio file lives in the package's Media/Audio
static bool allAudioLivesIn(AudiumEngine& engine, const File& package)
{
    auto audioDir = AudioResourceContainer::getAudioFileDirectory(package);
    auto tracks = engine.getAudioTrackContainer();
    auto anyResource = false;
    for (int i = 0; i < tracks->getNumItems(); ++i) {
        for (auto& resource : engine.getAudioResourceContainer()->getAudioResourcesForTrack(tracks->getAudioTrack(i).get())) {
            anyResource = true;
            auto file = resource->getUrl().getLocalFile();
            if (!file.existsAsFile() || file.getParentDirectory() != audioDir)
                return false;
        }
    }
    return anyResource;
}

SCENARIO("a failed Save As leaves neither a partial package nor a damaged source", "[engine][save][saveas]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    auto sourceSession = File(saveFailureTestFilesDirectory + "Sessions/simple-sine.audium");
    REQUIRE(sourceSession.exists());

    // open a disposable copy, never the checked-in session (see MoveChannelsTests)
    auto original = File::getSpecialLocation(File::tempDirectory)
                        .getChildFile("save-failure-original.audium")
                        .getNonexistentSibling();
    REQUIRE(sourceSession.copyDirectoryTo(original));
    const auto originalAudio = AudioResourceContainer::getAudioFileDirectory(original).getChildFile("sine-0dB.wav");
    REQUIRE(originalAudio.existsAsFile());
    const auto originalAudioSize = originalAudio.getSize();

    auto target = File::getSpecialLocation(File::tempDirectory)
                      .getChildFile("save-failure-target.audium")
                      .getNonexistentSibling();
    const auto targetProject = target.getChildFile(ProjectFileStore::projectFileName);

    GIVEN("an opened project") {
        REQUIRE(store->open(original, nullptr));
        REQUIRE(allAudioLivesIn(*engine, original));

        WHEN("saving as into a package whose Project.json can't be written (it is a directory)") {
            // the audio copy succeeds, the atomic JSON write is what fails
            REQUIRE(targetProject.createDirectory());

            std::string error;
            const auto saved = store->save(targetProject, [&error] (std::string message) { error = message; });

            THEN("the save fails with a reason") {
                REQUIRE_FALSE(saved);
                REQUIRE_FALSE(error.empty());
            }

            THEN("nothing of the attempt is left in the target") {
                REQUIRE_FALSE(target.getChildFile("Media").exists());
                REQUIRE(targetProject.isDirectory()); // the caller's directory is not ours to remove
            }

            THEN("the session still belongs to the original package") {
                REQUIRE(ProjectFileStore::projectDirectory == original);
                REQUIRE(store->getCurrentProjectFile() == original.getChildFile(ProjectFileStore::projectFileName));
                REQUIRE(allAudioLivesIn(*engine, original));

                AND_THEN("a plain save still goes to the original") {
                    REQUIRE(store->save(store->getCurrentProjectFile(), nullptr));
                    REQUIRE(ProjectFileStore::isValidProjectStructure(original));
                    REQUIRE(allAudioLivesIn(*engine, original));
                }
            }

            THEN("the original package is intact and loads") {
                REQUIRE(originalAudio.existsAsFile());
                REQUIRE(originalAudio.getSize() == originalAudioSize);
                REQUIRE(ProjectFileStore::isValidProjectStructure(original));
                REQUIRE(store->open(original, nullptr));
                REQUIRE(allAudioLivesIn(*engine, original));
            }
        }

        WHEN("saving as onto a path whose package is an existing regular file") {
            REQUIRE(target.replaceWithText("not a package"));

            std::string error;
            const auto saved = store->save(targetProject, [&error] (std::string message) { error = message; });

            THEN("the save fails, the file and the original are untouched") {
                REQUIRE_FALSE(saved);
                REQUIRE_FALSE(error.empty());
                REQUIRE(target.existsAsFile());
                REQUIRE(target.loadFileAsString() == "not a package");

                REQUIRE(ProjectFileStore::projectDirectory == original);
                REQUIRE(allAudioLivesIn(*engine, original));
                REQUIRE(originalAudio.existsAsFile());
                REQUIRE(store->open(original, nullptr));
            }
        }

       #if !JUCE_WINDOWS
        WHEN("saving as into a new package whose audio can't be copied") {
            // an unreadable source file fails the copy after the package was
            // created (permissions don't stop root, so this is skipped there)
            REQUIRE(chmod(originalAudio.getFullPathName().toRawUTF8(), 0) == 0);
            const auto permissionsHonoured = !originalAudio.hasReadAccess();

            std::string error;
            const auto saved = permissionsHonoured
                                   ? store->save(targetProject, [&error] (std::string message) { error = message; })
                                   : false;

            REQUIRE(chmod(originalAudio.getFullPathName().toRawUTF8(), 0644) == 0);

            THEN("the save fails and the new package is gone again") {
                if (!permissionsHonoured)
                    SKIP("running as root - unreadable files can't be provoked");

                REQUIRE_FALSE(saved);
                REQUIRE_FALSE(error.empty());
                REQUIRE_FALSE(target.exists());

                REQUIRE(ProjectFileStore::projectDirectory == original);
                REQUIRE(allAudioLivesIn(*engine, original));
                REQUIRE(originalAudio.getSize() == originalAudioSize);
                REQUIRE(store->open(original, nullptr));
            }
        }
       #endif

        WHEN("saving as into a fresh package succeeds") {
            REQUIRE(store->save(targetProject, nullptr));

            THEN("the package is complete and the original keeps its audio") {
                REQUIRE(ProjectFileStore::isValidProjectStructure(target));
                REQUIRE(allAudioLivesIn(*engine, target));
                REQUIRE(originalAudio.existsAsFile());
                REQUIRE(originalAudio.getSize() == originalAudioSize);
            }
        }
    }

    // cleanup ... comment out in case you need to isolate an issue
    target.deleteRecursively();
    original.deleteRecursively();

    engine = nullptr;
}
