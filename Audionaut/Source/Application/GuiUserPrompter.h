//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/Core/UserPrompter.h"

namespace audium {

/**
 * @class GuiUserPrompter
 * @brief The engine's UserPrompter for the GUI app: native message boxes
 *        and the native file chooser.
 *
 * Save dialogs open in the app's initialSaveDirectory and move it to where
 * the user saved, like the app's own Save As.
 */
class GuiUserPrompter : public UserPrompter
{
public:
    GuiUserPrompter() = default;

    void showMessage (const juce::String& title, const juce::String& message) override;

    bool confirm (const juce::String& title, const juce::String& message) override;

    void chooseFileToSave (const juce::String& title,
                           const juce::String& suggestedFileName,
                           const juce::String& wildcard,
                           std::function<void (const juce::File&)> onChosen) override;

private:
    // the chooser must outlive its dialog; one at a time, like the app's own
    std::shared_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuiUserPrompter)
};

} // namespace audium
