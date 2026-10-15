//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include <functional>
#include <iostream>

namespace audium {

/**
 * @class UserPrompter
 * @brief The engine's one way of talking to whoever is driving it.
 *
 * The engine occasionally has to tell the user something (an export failed)
 * or ask them something (may these files go to the trash? where should this
 * file go?). It never opens a window for that itself: it asks the prompter
 * its host installed. The GUI app installs one that shows the native dialogs;
 * everything else - tests, audionaut-cli, the in-app CLI mode, an agent host
 * - gets the `HeadlessUserPrompter`, which logs and answers as if nobody were
 * there.
 *
 * Message thread only.
 */
class UserPrompter
{
public:
    virtual ~UserPrompter() = default;

    /** Tells the user something; returns without waiting for them. */
    virtual void showMessage (const juce::String& title, const juce::String& message) = 0;

    /** Asks a yes/no question and waits for the answer. */
    virtual bool confirm (const juce::String& title, const juce::String& message) = 0;

    /**
     * Asks where to save a file. `onChosen` is called once, with a null file
     * when the user cancelled; whether that happens before this returns or
     * later depends on the host.
     *
     * @param suggestedFileName  A file name to offer, or empty for none.
     * @param wildcard           The file patterns to offer, e.g. "*.wav".
     */
    virtual void chooseFileToSave (const juce::String& title,
                                   const juce::String& suggestedFileName,
                                   const juce::String& wildcard,
                                   std::function<void (const juce::File&)> onChosen) = 0;
};

/**
 * @class HeadlessUserPrompter
 * @brief The prompter for sessions without a user: logs, and answers
 *        whatever leaves the project as it is.
 *
 * A question is declined, a save is cancelled, a message goes to stderr
 * (stdout belongs to the CLI's output). The CLI verbs rely on the decline:
 * remove-track and remove-channel promise that the audio files stay in the
 * package, and cleaning up is a GUI decision.
 */
class HeadlessUserPrompter : public UserPrompter
{
public:
    void showMessage (const juce::String& title, const juce::String& message) override
    {
        std::cerr << title << ": " << message << std::endl;
    }

    bool confirm (const juce::String& title, const juce::String&) override
    {
        std::cerr << title << " - declined (no user to ask)" << std::endl;
        return false;
    }

    void chooseFileToSave (const juce::String& title,
                           const juce::String&,
                           const juce::String&,
                           std::function<void (const juce::File&)> onChosen) override
    {
        std::cerr << title << " - cancelled (no user to ask)" << std::endl;
        juce::NullCheckedInvocation::invoke (onChosen, juce::File());
    }
};

} // namespace audium
