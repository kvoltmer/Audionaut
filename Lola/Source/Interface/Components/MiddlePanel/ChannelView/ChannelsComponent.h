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
#include "Interface/Models/ChannelGroupListBoxModel.h"
#include "Interface/Components/MiddlePanel/ChannelView/ChannelsHeaderComponent.h"
#include "Util/EngineAccess.h"
#include "Engine/Group/AudioTrackContainer.h"

class ChannelsComponent  : public juce::Component
{
public:
    ChannelsComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
        audiumEngine(audiumEngine)
    {
        audioChannelsListBox.reset(new audium::ListBox("Audio Group Listbox", nullptr));
        audioChannelsListBoxModel.reset(new ChannelGroupListBoxModel(audioChannelsListBox,
                                                                      audiumEngine));
        
        audioChannelsListBox->setModel(audioChannelsListBoxModel.get());
        
        auto headerComponent = std::unique_ptr<ChannelsHeaderComponent> (new ChannelsHeaderComponent(audiumEngine));
        audioChannelsListBox->setHeaderComponent(std::move(headerComponent));
        audioChannelsListBox->getHeaderComponent()->setSize(getWidth(), 25);
        audioChannelsListBox->setOutlineThickness(0);
        audioChannelsListBox->setMultipleSelectionEnabled(true);
        // hide scrollbars
        audioChannelsListBox->getViewport()->setScrollBarsShown(false, false);
        addAndMakeVisible(audioChannelsListBox.get());
    }

    ~ChannelsComponent() override
    {
        audioChannelsListBox->setModel(nullptr);
        audioChannelsListBox->setHeaderComponent(nullptr);
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
    
    void setVerticalScrollOffset(double offset)
    {
        // std::cout << "setVerticalScrollOffset " << offset << std::endl;
        auto range = audioChannelsListBox->getViewport()->getVerticalScrollBar().getCurrentRange().withStart(offset);
        audioChannelsListBox->getViewport()->getVerticalScrollBar().setCurrentRange(range, sendNotificationSync);
    }

private:
    std::shared_ptr<AudiumEngine>               audiumEngine;
    std::shared_ptr<audium::ListBox>            audioChannelsListBox;
    std::shared_ptr<ChannelGroupListBoxModel>  audioChannelsListBoxModel;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelsComponent)
};
