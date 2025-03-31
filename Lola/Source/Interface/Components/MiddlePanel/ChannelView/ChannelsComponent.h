//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/ColourIds.h"
#include "Interface/Models/ChannelGroupListBoxModel.h"
#include "Interface/Components/MiddlePanel/ChannelView/ChannelsHeaderComponent.h"
#include "Util/EngineAccess.h"
#include "Engine/Group/AudioTrackContainer.h"

class ChannelsComponent  : public juce::Component, public juce::DragAndDropTarget
{
public:
    ChannelsComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine) :
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
        audioChannelsListBox->setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    }

    ~ChannelsComponent() override
    {
        audioChannelsListBox->setModel(nullptr);
        audioChannelsListBox->setHeaderComponent(nullptr);
    }

    void paint (juce::Graphics& ) override;
    
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
    
    double getVerticalScrollOffset() const
    {
        return audioChannelsListBox->getViewport()->getVerticalScrollBar().getCurrentRange().getStart();
    }
    
    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) override;
    void itemDragEnter (const SourceDetails &dragSourceDetails) override;
    void itemDragMove (const SourceDetails &dragSourceDetails) override;
    void itemDragExit (const SourceDetails &dragSourceDetails) override;
    void itemDropped (const SourceDetails &dragSourceDetails) override;
    bool shouldDrawDragImageWhenOver () override
    {
        return true;
    }

private:
    
    int getListBoxHeight() const;
    
    std::shared_ptr<audium::AudiumEngine>               audiumEngine;
    std::shared_ptr<audium::ListBox>            audioChannelsListBox;
    std::shared_ptr<ChannelGroupListBoxModel>  audioChannelsListBoxModel;
    
    bool itemDrag = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelsComponent)
};
