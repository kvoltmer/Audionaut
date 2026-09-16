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
        anything is written; the outcome is not reported back. The chooser
        must outlive the dialog (the callers keep it as a member). */
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
                
                
                // runs the export with its progress window; false when the
                // user cancels, which leaves nothing to do here
                exportThread->runThread();

#if !defined(AUDIONAUT_HEADLESS)
                AudiumApplication::getApp().initialSaveDirectory = file.getParentDirectory();
#endif
            }
        });
    }
};

} // namespace audium
