/*
  ==============================================================================

 ExportAudioDialog.h
    Created: 1 Jun 2023 4:23:13pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <iostream>
#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/Export/ExportAudioConfig.h"
#include "Interface/Components/MainComponent.h"
#include "Interface/Dialogs/ExportAudioComponent.h"
#include "Application/AudiumApplication.h"
#include "Engine/Export/AudioExportThread.h"

using namespace juce;

class ExportAudioDialog
{
    
public:
    ExportAudioDialog(std::shared_ptr<AudiumEngine> engine) :
        audiumEngine(engine)
    {
        exportAudioComponent.reset(new ExportAudioComponent(engine));
    }
    
    void invoke(std::shared_ptr<MainComponent> component)
    {
        invokeInternal(component);
    }
    
private:
    
    static String getClassNameFieldName()  { return "Auto Edit Name"; }
    
    void invokeInternal(std::shared_ptr<MainComponent> component)
    {
        mainComponent = component;
        asyncAlertWindow = std::make_unique<AlertWindow> (TRANS ("Export Audio"),
                                                          "",
                                                          MessageBoxIconType::NoIcon, mainComponent.get());
        asyncAlertWindow->addCustomComponent(exportAudioComponent.get());
        asyncAlertWindow->addButton (TRANS ("Export..."),  1, KeyPress (KeyPress::returnKey));
        asyncAlertWindow->addButton (TRANS ("Cancel"), 0, KeyPress (KeyPress::escapeKey));
        exportAudioComponent->update();
        
        auto resultCallback = [safeThis = WeakReference<ExportAudioDialog> { this }, this] (int result)
        {
            if (safeThis == nullptr)
                return;

            auto& aw = *(safeThis->asyncAlertWindow);

            aw.exitModalState (result);
            aw.setVisible (false);

            if (result == 0)
                return;
            
            // get the sample rate
            auto sr = exportAudioComponent->getSampleRate().toString().getIntValue();
            safeThis->config.sampleRate = (double) sr;
            
            // get the number of output channels
            auto chans = exportAudioComponent->getOutputChannels().toString().getIntValue();
            if (chans == 3 || chans == 4) {
                if (chans == 4) {
                    safeThis->config.multiMono = true;
                }
                chans = audiumEngine->getAudioTrackContainer()->getNumAudioTrackChannels();
            }
            safeThis->config.numChannels = chans;
            

            
            safeThis->exportAudio();
        };

        asyncAlertWindow->enterModalState (true, ModalCallbackFunction::create (std::move (resultCallback)), false);
    }
    
    void exportAudio()
    {
        auto dir = AudiumApplication::getApp().initialSaveDirectory;
        
        chooser = std::make_unique<FileChooser> (("Export As Wav File. Choose a filename..."), dir, "*.wav");
        auto flags = FileBrowserComponent::saveMode
                   | FileBrowserComponent::canSelectFiles
                   | FileBrowserComponent::warnAboutOverwriting;

        chooser->launchAsync (flags, [this] (const FileChooser& fc)
        {
            const auto result = fc.getResult();
            
            if (result != File{})
            {
                // assign the choosen filename
                config.fileName = result;
                
                // create the thread
                auto thread = std::make_unique<AudioExportThread>(*audiumEngine.get(), config);
                
                // start the thread
                if (thread->runThread())
                {
                    // thread finished normally..
                }
                else
                {
                    // user pressed the cancel button..
                }
            }
        });
    }
    
    std::unique_ptr<AlertWindow> asyncAlertWindow;
    std::unique_ptr<ExportAudioComponent> exportAudioComponent;

    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<MainComponent> mainComponent;
    
    std::unique_ptr<juce::FileChooser> chooser;
    
    
public:
    audium::ExportAudioConfig config;
    
private:
    JUCE_DECLARE_WEAK_REFERENCEABLE (ExportAudioDialog)
};
