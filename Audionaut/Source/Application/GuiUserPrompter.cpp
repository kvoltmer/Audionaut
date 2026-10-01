//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "GuiUserPrompter.h"
#include "AudiumApplication.h"

namespace audium {

void GuiUserPrompter::showMessage (const juce::String& title, const juce::String& message)
{
    juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, title, message);
}

bool GuiUserPrompter::confirm (const juce::String& title, const juce::String& message)
{
    return juce::NativeMessageBox::showYesNoBox (juce::MessageBoxIconType::WarningIcon, title, message);
}

void GuiUserPrompter::chooseFileToSave (const juce::String& title,
                                        const juce::String& suggestedFileName,
                                        const juce::String& wildcard,
                                        std::function<void (const juce::File&)> onChosen)
{
    auto& app = AudiumApplication::getApp();

    auto startAt = app.initialSaveDirectory;
    if (suggestedFileName.isNotEmpty())
        startAt = startAt.getChildFile (suggestedFileName);

    chooser = std::make_shared<juce::FileChooser> (title, startAt, wildcard);

    const auto flags = juce::FileBrowserComponent::saveMode
                     | juce::FileBrowserComponent::canSelectFiles
                     | juce::FileBrowserComponent::warnAboutOverwriting;

    chooser->launchAsync (flags, [onChosen] (const juce::FileChooser& fc) {
        const auto file = fc.getResult();

        // remember where the user went, unless it is somewhere they cannot
        // save to anyway (a sandboxed app is confined to the Music folder)
        if (file != juce::File() && file.hasWriteAccess())
            AudiumApplication::getApp().initialSaveDirectory = file.getParentDirectory();

        juce::NullCheckedInvocation::invoke (onChosen, file);
    });
}

} // namespace audium
