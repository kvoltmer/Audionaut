//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Widgets/audium_ListBox.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioSubGroup.h"
#include "Engine/Channel/AudioChannel.h"

class SubGroupListBoxModel  : public audium::ListBoxModel
{
public:
    SubGroupListBoxModel(std::shared_ptr<audium::ListBox> owner,
                         std::shared_ptr<audium::AudioSubGroup> audioSubGroup,
                         std::shared_ptr<audium::AudiumEngine> audiumEngine,
                         std::shared_ptr<ZoomHandler> zoomHandler,
                         std::shared_ptr<RegionSelector> regionSelector) :
        owner(owner),
        audioSubGroup(audioSubGroup),
        audiumEngine(audiumEngine),
        zoomHandler(zoomHandler),
        regionSelector(regionSelector)
    {
    }
    
    ~SubGroupListBoxModel() override
    {
    }
    
    int getNumRows() override
    {
        // std::cout << audioSubGroup->getNumChannels() << std::endl;
        return audioSubGroup->getNumChannels();
    }

    void paintListBoxItem ( int rowNumber,
                            juce::Graphics& g,
                            int width, int height,
                            bool rowIsSelected) override
    {
        auto channel = audioSubGroup->getAudioTrack().getChannel(rowNumber);
        
        if (channel != nullptr &&
            channel->isSelected())
        {
            g.setColour (owner->findColour(audium::secondaryBackgroundColourId).brighter().withAlpha(0.9f));
            g.fillRect(juce::Rectangle<int>(0, 0, width, height));
        }
        
    }
    
    juce::Component* refreshComponentForRow (   int rowNumber, bool isRowSelected,
                                                juce::Component* existingComponentToUpdate) override
    {
    
        auto audioResource = audioSubGroup->getAudioResourceAtChannel(rowNumber);
        if (existingComponentToUpdate == nullptr)
        {
            if (audioResource != nullptr)
            {
                auto component = new AudioResourceView(owner.get(),
                                                       audiumEngine,
                                                       audioResource,
                                                       zoomHandler,
                                                       audioSubGroup->getAudioTrack().getColour(),
                                                       regionSelector,
                                                       rowNumber);
                return component;
            }
            else
            {
                return new juce::Component();
            }
        }
        else
        {
            auto component = dynamic_cast<AudioResourceView*>(existingComponentToUpdate);
            if (component != nullptr)
            {
                component->updateUI(rowNumber);
                return component;
            }
            else
            {
                return existingComponentToUpdate;
            }
        }
        
        return nullptr;
    }

    
    int getRowHeight (int rowNumber) const override
    {
        auto channel = audioSubGroup->getAudioTrack().getChannel(rowNumber);
        if (channel != nullptr)
        {
            return channel->getChannelHeight();
        }
        else
        {
            return 100;
        }
    }
    
    void deleteKeyPressed (int lastRowSelected) override
    {
    }
    
    void backgroundClicked (const juce::MouseEvent&) override
    {
        owner->deselectAllRows();
    }
    
    void listWasScrolled() override {}
    
    void selectedRowsChanged (int lastRowSelected) override
    {
    }
    
    void setAudioSubGroup(std::shared_ptr<audium::AudioSubGroup> subGroup)
    {
        audioSubGroup = subGroup;
    }
    
        
private:
    
    std::shared_ptr<audium::ListBox> owner;
    std::shared_ptr<audium::AudioSubGroup> audioSubGroup;
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<RegionSelector> regionSelector;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubGroupListBoxModel)
};
