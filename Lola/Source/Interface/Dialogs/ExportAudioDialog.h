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
#include "Engine/ExportAudioConfig.h"
#include "Interface/Components/MainComponent.h"
#include "Interface/Dialogs/ExportAudioComponent.h"
#include "Application/AudiumApplication.h"

using namespace juce;

class ExportAudioDialog
{
    
public:
    ExportAudioDialog()
    {
        exportAudioComponent.reset(new ExportAudioComponent());
    }
    
    void invoke(std::shared_ptr<AudiumEngine> engine, std::shared_ptr<MainComponent> component)
    {
        invokeInternal(engine, component);
    }
    
private:
    
    static String getClassNameFieldName()  { return "Auto Edit Name"; }
    
    void invokeInternal(std::shared_ptr<AudiumEngine> engine,
                                std::shared_ptr<MainComponent> component)
    {
        audiumEngine = engine;
        mainComponent = component;
        asyncAlertWindow = std::make_unique<AlertWindow> (TRANS ("Parameters:"),
                                                          "",
                                                          MessageBoxIconType::NoIcon, mainComponent.get());

        asyncAlertWindow->addCustomComponent(exportAudioComponent.get());
        asyncAlertWindow->addButton (TRANS ("Export"),  1, KeyPress (KeyPress::returnKey));
        asyncAlertWindow->addButton (TRANS ("Cancel"), 0, KeyPress (KeyPress::escapeKey));
        



        auto resultCallback = [safeThis = WeakReference<ExportAudioDialog> { this }, this] (int result)
        {
            if (safeThis == nullptr)
                return;

            auto& aw = *(safeThis->asyncAlertWindow);

            aw.exitModalState (result);
            aw.setVisible (false);

            if (result == 0)
                return;
            
            //auto mode = autoEditComponent->getEditMode().toString().getIntValue();
            
            safeThis->config.sampleRate = 96000.0;
            safeThis->exportAudio();
        };

        asyncAlertWindow->enterModalState (true, ModalCallbackFunction::create (std::move (resultCallback)), false);
        auto editor = asyncAlertWindow->getTextEditor(getClassNameFieldName());
        if (editor != nullptr)
            editor->toFront(true);
    }
    
    void exportAudio()
    {
        auto dir = AudiumApplication::getApp().initialSaveDirectory;
        
        chooser = std::make_unique<FileChooser> ((" Export As Wav File."), dir, "*.wav");
        auto flags = FileBrowserComponent::saveMode
                   | FileBrowserComponent::canSelectFiles
                   | FileBrowserComponent::warnAboutOverwriting;

        chooser->launchAsync (flags, [this] (const FileChooser& fc)
        {
            const auto result = fc.getResult();
            
            if (result != File{})
            {
                config.fileName = result;
                audiumEngine->bounceToFile(config);
            }
        });
        
        //audiumEngine->bounceToFile(<#const juce::File &f#>, <#std::function<void (bool)> callback#>, <#double preferedSampleRate#>)
    }
    
    std::unique_ptr<AlertWindow> asyncAlertWindow;
    std::unique_ptr<ExportAudioComponent> exportAudioComponent;

    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<MainComponent> mainComponent;
    
    std::unique_ptr<juce::FileChooser> chooser;
    audium::ExportAudioConfig config;
    
    JUCE_DECLARE_WEAK_REFERENCEABLE (ExportAudioDialog)
};
