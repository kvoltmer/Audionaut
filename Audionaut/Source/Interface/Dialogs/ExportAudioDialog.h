//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <iostream>
#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/Export/ExportAudioConfig.h"
#include "Interface/Components/MainComponent.h"
#include "Interface/Dialogs/ExportAudioComponent.h"
#include "Application/AudiumApplication.h"
#include "Engine/Export/AudioExportThread.h"
#include "Engine/Export/ExportUtil.h"

using namespace juce;

class ExportAudioDialog
{
    
public:
    ExportAudioDialog(std::shared_ptr<audium::AudiumEngine> engine) :
        audiumEngine(engine)
    {
        exportAudioComponent.reset(new ExportAudioComponent(engine));
    }
    
    void invoke(std::shared_ptr<MainComponent> component)
    {
        invokeInternal(component);
    }
    
private:
    
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
            
            safeThis->config = std::make_shared<audium::ExportAudioConfig>();
            
            // get the sample rate
            safeThis->config->sampleRate = exportAudioComponent->getSampleRate().toString().getDoubleValue();
            
            // get the number of output channels
            auto chans = exportAudioComponent->getOutputChannels().toString().getIntValue();
            if (chans == 3 || chans == 4) {
                if (chans == 4) {
                    safeThis->config->multiMono = true;
                }
                chans = audiumEngine->getAudioTrackContainer()->getNumAudioTrackChannels();
            }
            safeThis->config->numChannels = chans;
            
            safeThis->config->bitDepth = exportAudioComponent->getBitDepth().toString().getIntValue();
            juce::File dir;
        #if !defined(CATCH2_TESTS)
            dir = AudiumApplication::getApp().initialSaveDirectory;
        #endif
            safeThis->chooser = std::make_shared<FileChooser> (("Export as WAV file. Choose a filename..."), dir, "*.wav");
            audium::ExportUtil::exportAudio(safeThis->chooser, audiumEngine, safeThis->config);


        };

        asyncAlertWindow->enterModalState (true, ModalCallbackFunction::create (std::move (resultCallback)), false);
    }
    
    std::unique_ptr<AlertWindow> asyncAlertWindow;
    std::unique_ptr<ExportAudioComponent> exportAudioComponent;

    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<MainComponent> mainComponent;
    
    std::shared_ptr<juce::FileChooser> chooser;
    
    
public:
    std::shared_ptr<audium::ExportAudioConfig> config;
    
private:
    JUCE_DECLARE_WEAK_REFERENCEABLE (ExportAudioDialog)
};
