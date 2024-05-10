/*
  ==============================================================================

    ChannelGroupComponent.h
    Created: 24 Dec 2023 11:19:56am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Models/ChannelSubGroupListBoxModel.h"
#include "Interface/Components/MiddlePanel/ChannelView/ChannelGroupHeaderComponent.h"

#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioGroup.h"

//==============================================================================
/*
    Display a group and it's channels
 
*/
class ChannelGroupComponent  : public juce::Component
{
public:
    ChannelGroupComponent(std::shared_ptr<AudioGroup> audioGroup,
                          std::shared_ptr<AudiumEngine> audiumEngine) :
        audioGroup(audioGroup),
        audiumEngine(audiumEngine)
    {
        channelsListBox.reset(new audium::ListBox());
        channelsListBoxModel.reset(new ChannelSubGroupListBoxModel(*channelsListBox.get(), audiumEngine, audioGroup));
        channelsListBox->setModel(channelsListBoxModel.get());
        
        
        auto header = std::unique_ptr<ChannelGroupHeaderComponent>(new ChannelGroupHeaderComponent(audioGroup));
        channelsListBox->setHeaderComponent(std::move(header));
        channelsListBox->getHeaderComponent()->setSize(getWidth(), DraggerControl::draggerHeight);
        channelsListBox->setOutlineThickness(0);
        channelsListBox->setMultipleSelectionEnabled(true);
        addAndMakeVisible(channelsListBox.get());
        
    }

    ~ChannelGroupComponent() override
    {
        audioGroup = nullptr;
        channelsListBox->setModel(nullptr);
    }

    void paint (juce::Graphics& g) override
    {
    }

    void resized() override
    {
        channelsListBox->setBounds(getLocalBounds());
    }
    
    void refreshComponent(std::shared_ptr<AudioGroup> audioGroup)
    {
        if (this->audioGroup != audioGroup)
        {
            channelsListBoxModel->setAudioGroup(audioGroup);
            this->audioGroup = audioGroup;
        }
        channelsListBox->updateContent();
        channelsListBox->setSelectedRows(audioGroup->getSelectedRows());
    }

private:
    
    std::shared_ptr<AudioGroup> audioGroup;
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    std::unique_ptr<audium::ListBox> channelsListBox;
    std::unique_ptr<ChannelSubGroupListBoxModel> channelsListBoxModel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelGroupComponent)
};
