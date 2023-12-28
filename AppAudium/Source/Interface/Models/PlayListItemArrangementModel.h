/*
  ==============================================================================

    PlayListItemArrangementModel.h
    Created: 23 Oct 2023 3:14:04pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/Group/AudioSubGroup.h"
#include "Engine/Channel/AudioChannel.h"

#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Views/AudioRegionView.h"


class PlayListItemArrangementModel  : public audium::ListBoxModel
{
public:
    PlayListItemArrangementModel(audium::ListBox& owner,
                                 std::shared_ptr<AudioGroup> audioGroup,
                                 std::shared_ptr<PlayListItem> playListItem,
                                 std::shared_ptr<AudiumEngine> audiumEngine,
                                 std::shared_ptr<ZoomHandler> zoomHandler,
                                 std::shared_ptr<RegionSelector> regionSelector) :
        owner(owner),
        audioGroup(audioGroup),
        playListItem(playListItem),
        audiumEngine(audiumEngine),
        zoomHandler(zoomHandler),
        regionSelector(regionSelector)
    {
    }
    
    ~PlayListItemArrangementModel() override
    {
    }
    
    int getNumRows() override
    {
        auto subGroup = playListItem->getRegion()->getAudioSubGroup();
        return subGroup->getNumChannels();
    }

    void paintListBoxItem ( int rowNumber,
                            juce::Graphics& g,
                            int width, int height,
                            bool rowIsSelected) override
    {
    }
    
    juce::Component* refreshComponentForRow (   int rowNumber, bool isRowSelected,
                                                juce::Component* existingComponentToUpdate) override
    {
    
        auto audioSubGroup = playListItem->getRegion()->getAudioSubGroup();
        auto audioResource = audioSubGroup->getChannel(rowNumber);
        
        if (existingComponentToUpdate == nullptr)
        {
            if (audioResource != nullptr)
            {
                
                auto component = new AudioRegionView(audiumEngine,
                                                     audioResource,
                                                     zoomHandler,
                                                     playListItem->getRegion(),
                                                     audioGroup->getColour(),
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
            auto component = dynamic_cast<AudioRegionView*>(existingComponentToUpdate);
            if (component != nullptr)
            {
                // TODO: row might have changed after delete
                component->updateFromEngine();
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
        // auto channel = audioSubGroup->getAudioGroup().getChannel(rowNumber);
        auto channel = audioGroup->getChannel(rowNumber);
        return channel->getChannelHeight();
    }
    
    void deleteKeyPressed (int lastRowSelected) override
    {
        // TODO: implement
    }
    
    void backgroundClicked (const juce::MouseEvent&) override
    {
        owner.deselectAllRows();
    }
    
    void listWasScrolled() override {}
    
    void selectedRowsChanged (int lastRowSelected) override
    {
    }
        
private:
    
    audium::ListBox& owner;
    std::shared_ptr<AudioGroup> audioGroup;
    std::shared_ptr<PlayListItem> playListItem;
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<RegionSelector> regionSelector;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListItemArrangementModel)
};
