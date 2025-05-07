//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/ResourceGroup.h"
#include "Engine/Channel/AudioChannel.h"

#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Views/AudioRegionView.h"


class PlayListItemArrangementModel  : public audium::ListBoxModel
{
public:
    PlayListItemArrangementModel(audium::ListBox& owner,
                                 std::shared_ptr<audium::AudioTrack> audioTrack,
                                 std::shared_ptr<audium::PlayListItem> playListItem,
                                 std::shared_ptr<audium::AudiumEngine> audiumEngine,
                                 std::shared_ptr<ZoomHandler> zoomHandler,
                                 std::shared_ptr<RegionSelector> regionSelector) :
        owner(owner),
        audioTrack(audioTrack),
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
        auto resourceGroup = playListItem->getRegion()->getResourceGroup();
        return resourceGroup->getNumChannels();
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
        auto resourceGroup = playListItem->getRegion()->getResourceGroup();
        auto audioResource = resourceGroup->getAudioResourceAtChannel(rowNumber);
        
        if (existingComponentToUpdate == nullptr) {
            if (audioResource != nullptr) {
                jassert(parentComponent);
                jassert(owner.getParentComponent()->getParentComponent() == parentComponent);
                auto component = new AudioRegionView(parentComponent,
                                                     audiumEngine,
                                                     audioResource,
                                                     zoomHandler,
                                                     audioTrack->getColour(),
                                                     regionSelector,
                                                     rowNumber);
                component->setPlayListItem(playListItem);
                return component;
            }
            else {
                return new juce::Component();
            }
        }
        else {
            if (auto component = dynamic_cast<AudioRegionView*>(existingComponentToUpdate)) {
                jassert(owner.getParentComponent()->getParentComponent() == parentComponent);
                component->setParentComponent(parentComponent);
                component->setPlayListItem(playListItem);
                component->updateUI(rowNumber);
                return component;
            }
        }
        
        return nullptr;
    }

    
    int getRowHeight (int rowNumber) const override
    {
        auto channel = audioTrack->getChannel(rowNumber);
        if (channel != nullptr)
        {
            return channel->getChannelHeight();
        }
        return 0;
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
    
    void setPlayListItem(std::shared_ptr<audium::PlayListItem> playListItem_)
    {
        playListItem = playListItem_;
    }
    
    void setParentComponent(juce::Component *comp)
    {
        parentComponent = comp;
    }
    
private:
    
    audium::ListBox& owner;
    juce::Component *parentComponent = nullptr;
    std::shared_ptr<audium::AudioTrack> audioTrack;
    std::shared_ptr<audium::PlayListItem> playListItem;
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<RegionSelector> regionSelector;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListItemArrangementModel)
};
