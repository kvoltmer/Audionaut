/*
  ==============================================================================

    ExportUtil.h
    Created: 19 Sep 2025 8:39:59am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"

namespace audium {

class ExportUtil {
    
    
public:
    
    static void exportAudio(std::shared_ptr<juce::FileChooser> chooser,
                            std::shared_ptr<audium::AudiumEngine> audiumEngine,
                            std::shared_ptr<audium::ExportAudioConfig> config)
    {
        
        jassert(chooser);
        auto flags = FileBrowserComponent::saveMode
                   | FileBrowserComponent::canSelectFiles
                   | FileBrowserComponent::warnAboutOverwriting;

        chooser->launchAsync (flags, [audiumEngine, config] (const FileChooser& fc) {
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
                
                // create the thread
                auto thread = std::make_unique<audium::AudioExportThread>(*audiumEngine.get(), config);
                
                // start the thread
                if (thread->runThread()) {
                    // thread finished normally..
                }
                else {
                    // user pressed the cancel button..
                }
                
                AudiumApplication::getApp().initialSaveDirectory = file.getParentDirectory();

            }
        });
    }
};

} // namespace audium
