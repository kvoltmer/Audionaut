#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Core/UserPrompter.h"

using namespace audium;

namespace {

// A host that remembers what it was asked and answers as told.
struct RecordingPrompter : public UserPrompter
{
    bool answer = false;
    std::vector<juce::String> questions;
    std::vector<juce::String> messages;

    void showMessage (const juce::String& title, const juce::String& message) override
    {
        messages.push_back (title + ": " + message);
    }

    bool confirm (const juce::String& title, const juce::String& message) override
    {
        questions.push_back (title + "\n" + message);
        return answer;
    }

    void chooseFileToSave (const juce::String&, const juce::String&, const juce::String&,
                           std::function<void (const juce::File&)> onChosen) override
    {
        juce::NullCheckedInvocation::invoke (onChosen, juce::File());
    }
};

juce::File makeStrayAudioDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("audionaut-prompter-test-" + juce::String (juce::Random::getSystemRandom().nextInt (1 << 30)));
    dir.deleteRecursively();
    REQUIRE (dir.createDirectory());
    REQUIRE (dir.getChildFile ("stray.wav").replaceWithText ("not really audio"));
    return dir;
}

} // namespace

SCENARIO("a session without a user leaves the project as it is", "[engine][prompter]")
{
    GIVEN("the headless prompter") {
        HeadlessUserPrompter prompter;

        THEN("a question is declined") {
            REQUIRE_FALSE (prompter.confirm ("Trash?", "Move 3 files to trash?"));
        }

        THEN("a save is cancelled, right away") {
            bool answered = false;
            juce::File chosen ("/never/replaced");
            prompter.chooseFileToSave ("Export", "take.wav", "*.wav", [&] (const juce::File& file) {
                answered = true;
                chosen = file;
            });
            REQUIRE (answered);
            REQUIRE (chosen == juce::File());
        }
    }
}

SCENARIO("the engine asks its host before moving the user's files to the trash", "[engine][prompter][resource]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock (juce::Thread::getCurrentThread());

    std::shared_ptr<AudiumEngine> engine;
    auto dir = makeStrayAudioDirectory();
    auto stray = dir.getChildFile ("stray.wav");

    GIVEN("a directory holding an audio file no resource refers to") {
        REQUIRE (stray.existsAsFile());

        WHEN("an engine built without a host looks for obsolete files there") {
            engine = AudiumFactory::createAudiumEngine();

            THEN("it got the headless prompter and the file stays") {
                REQUIRE (dynamic_cast<HeadlessUserPrompter*> (engine->getUserPrompter().get()) != nullptr);
                engine->getAudioResourceContainer()->deleteObsoleteAudioFiles (dir);
                REQUIRE (stray.existsAsFile());
            }
        }

        WHEN("the host declines") {
            auto host = std::make_shared<RecordingPrompter>();
            host->answer = false;
            engine = AudiumFactory::createAudiumEngine (host);
            REQUIRE (engine->getUserPrompter() == host);

            engine->getAudioResourceContainer()->deleteObsoleteAudioFiles (dir);

            THEN("it was asked once, naming the count, and the file stays") {
                REQUIRE (host->questions.size() == 1);
                REQUIRE (host->questions.front().contains ("1 files"));
                REQUIRE (host->questions.front().contains (stray.getFullPathName()));
                REQUIRE (host->messages.empty());
                REQUIRE (stray.existsAsFile());
            }
        }

        WHEN("the host agrees") {
            auto host = std::make_shared<RecordingPrompter>();
            host->answer = true;
            engine = AudiumFactory::createAudiumEngine (host);

            engine->getAudioResourceContainer()->deleteObsoleteAudioFiles (dir);

            THEN("the file is gone, or the host heard that trashing it failed") {
                REQUIRE (host->questions.size() == 1);
                if (stray.existsAsFile()) // no trash on this machine (a bare CI runner)
                    REQUIRE (host->messages.size() == 1);
                else
                    REQUIRE (host->messages.empty());
            }
        }

        WHEN("there is nothing obsolete") {
            auto host = std::make_shared<RecordingPrompter>();
            engine = AudiumFactory::createAudiumEngine (host);
            REQUIRE (stray.deleteFile());

            engine->getAudioResourceContainer()->deleteObsoleteAudioFiles (dir);

            THEN("the host is not bothered") {
                REQUIRE (host->questions.empty());
            }
        }
    }

    dir.deleteRecursively();
    engine = nullptr;
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}
