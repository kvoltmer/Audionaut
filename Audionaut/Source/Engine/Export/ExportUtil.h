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
    
    /** Asks the user for a target file (through the engine's UserPrompter)
        and runs the export once they have chosen. Asynchronous: this returns
        as soon as the question is asked, before anything is written; the
        outcome is not reported back to the caller, a failure is shown to
        the user instead.
        @param suggestedFileName  the file name to offer, or empty for none */
    static void exportAudio(const juce::String& suggestedFileName,
                            std::shared_ptr<audium::AudiumEngine> audiumEngine,
                            std::shared_ptr<audium::ExportAudioConfig> config,
                            std::shared_ptr<audium::AudioExportThread> exportThread)
    {
        auto prompter = audiumEngine->getUserPrompter();

        prompter->chooseFileToSave ("Export as WAV file. Choose a filename...", suggestedFileName, "*.wav",
                                    [prompter, audiumEngine, config, exportThread] (const File& file) {

            if (file != File{}) {
                
                if (!file.hasWriteAccess()) {
                    std::string errorString = "No write access. Please select a different location.";
        #if JUCE_MAC
                    errorString += "\n\n";
                    errorString += "As a 'Sandboxed App' you are only allowed to save files in the Music folder.";
        #endif
                    prompter->showMessage("Error",
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
                    prompter->showMessage("Error",
                                          "Failed to export " + file.getFullPathName() + "\n\n" + config->error);
            }
        });
    }
};

} // namespace audium
