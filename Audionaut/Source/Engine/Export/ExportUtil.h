//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"

namespace audium {

class ExportUtil {
    
    
public:
    
    /** Asks for a target file and runs the export once the chooser closes.
        Asynchronous: this returns as soon as the chooser is up, before
        anything is written; the outcome is not reported back to the caller,
        a failure is shown to the user instead. The chooser must outlive the
        dialog (the callers keep it as a member). */
    static void exportAudio(std::shared_ptr<juce::FileChooser> chooser,
                            std::shared_ptr<audium::AudiumEngine> audiumEngine,
                            std::shared_ptr<audium::ExportAudioConfig> config,
                            std::shared_ptr<audium::AudioExportThread> exportThread)
    {
        jassert(chooser);
        auto flags = FileBrowserComponent::saveMode
                   | FileBrowserComponent::canSelectFiles
                   | FileBrowserComponent::warnAboutOverwriting;

        chooser->launchAsync (flags, [audiumEngine, config, exportThread] (const FileChooser& fc) {
            const auto file = fc.getResult();
            
            if (file != File{}) {
                
                if (!file.hasWriteAccess()) {
                    std::string errorString = "No write access. Please select a different location.";
        #if JUCE_MAC
                    errorString += "\n\n";
                    errorString += "As a 'Sandboxed App' you are only allowed to save files in the Music folder.";
        #endif
                    juce::NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                          "Error",
                                                          "Failed to save " + file.getFullPathName() +"\n\n" + String(errorString));
                    return;
                }
                
                // assign the choosen filename
                jassert(config != nullptr);
                config->fileName = file;
                
                
                // runs the export with its progress window; a cancel leaves
                // nothing to do here, anything else that kept the file from
                // being written is reported
                exportThread->runThread();

                if (! exportThread->wasSuccessful() && ! config->userCanceled)
                    juce::NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                                "Error",
                                                                "Failed to export " + file.getFullPathName() + "\n\n" + config->error);

#if !defined(AUDIONAUT_HEADLESS)
                AudiumApplication::getApp().initialSaveDirectory = file.getParentDirectory();
#endif
            }
        });
    }
};

} // namespace audium
