/*
  ==============================================================================

    ChannelsComponent.h
    Created: 23 Oct 2023 12:02:02pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/ColourIds.h"
#include "Interface/Models/AudioChannelsListBoxModel.h"
#include "Util/EngineAccess.h"
#include "Engine/AudioGroupContainer.h"

class ChannelsComponent  : public juce::Component
{
public:
    ChannelsComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
        audiumEngine(audiumEngine)
    {
        audioChannelsListBox.reset(new audium::ListBox("Audio Group Listbox", nullptr));
        audioChannelsListBoxModel.reset(new AudioChannelsListBoxModel(audioChannelsListBox,
                                                                      audiumEngine));
        
        audioChannelsListBox->setModel(audioChannelsListBoxModel.get());
        
        auto headerComponent = std::unique_ptr<juce::Component> (new juce::Component());
        audioChannelsListBox->setHeaderComponent(std::move(headerComponent));
        audioChannelsListBox->getHeaderComponent()->setSize(getWidth(), 25);
        audioChannelsListBox->setOutlineThickness(0);
        audioChannelsListBox->setMultipleSelectionEnabled(true);
        addAndMakeVisible(audioChannelsListBox.get());
    }

    ~ChannelsComponent() override
    {
        audioChannelsListBox->setHeaderComponent(nullptr);
        audioChannelsListBox->setModel(nullptr);
    }

    void paint (juce::Graphics& ) override
    {
    }

    void resized() override
    {
        audioChannelsListBox->setBounds(getLocalBounds());
    }
    
    void updateUI()
    {
        audioChannelsListBox->updateContent();
    }


private:
    std::shared_ptr<AudiumEngine>               audiumEngine;
    std::shared_ptr<audium::ListBox>            audioChannelsListBox;
    std::shared_ptr<AudioChannelsListBoxModel>  audioChannelsListBoxModel;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelsComponent)
};
