//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/Export/ExportFormat.h"
#include "Engine/PlayList/PlayListScheduler.h"

namespace audium {

class ExportUtil {
    
    
public:
    
    /** Asks the user for a target file (through the engine's UserPrompter)
        and runs the export once they have chosen. Asynchronous: this returns
        as soon as the question is asked, before anything is written; the
        outcome is not reported back to the caller, a failure is shown to
        the user instead.
        @param suggestedFileName  the file name to offer, or empty for none
        @param formatFromChosenFile  offer every export format and take the
               one the chosen file's extension names (config->bitDepth is
               then reduced to what that format can hold); otherwise the
               chooser offers config->format only */
    static void exportAudio(const juce::String& suggestedFileName,
                            std::shared_ptr<audium::AudiumEngine> audiumEngine,
                            std::shared_ptr<audium::ExportAudioConfig> config,
                            std::shared_ptr<audium::AudioExportThread> exportThread,
                            bool formatFromChosenFile = false)
    {
        auto prompter = audiumEngine->getUserPrompter();

        auto title = formatFromChosenFile
                   ? juce::String ("Export as WAV, FLAC, AIFF, Ogg Vorbis or MP3 file. Choose a filename...")
                   : "Export as " + formatName (config->format) + " file. Choose a filename...";
        auto wildcard = formatFromChosenFile
                      ? exportWildcard()
                      : "*" + fileExtension (config->format);

        prompter->chooseFileToSave (title, suggestedFileName, wildcard,
                                    [prompter, audiumEngine, config, exportThread, formatFromChosenFile] (const File& chosenFile) {

            if (chosenFile != File{}) {

                // the file's extension and the format written must agree
                auto file = chosenFile;
                if (formatFromChosenFile) {
                    config->format = exportFormatForFile (file).value_or (ExportFormat::wav);
                    config->bitDepth = closestSupportedBitDepth (config->format, config->bitDepth);
                }
                if (exportFormatForFile (file) != config->format)
                    file = file.withFileExtension (fileExtension (config->format));
                
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
                
                
                // Flagged from here, on the message thread, not only from the
                // worker: the progress window's modal loop keeps dispatching
                // the project monitor and agent requests, and a reload that
                // landed before the worker got going would rebuild the graph
                // it is about to walk.
                const audium::PlayListScheduler::ScopedOfflineRender offlineRender (*audiumEngine->getPlayListScheduler());

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
