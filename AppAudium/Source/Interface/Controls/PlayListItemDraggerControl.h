/*
  ==============================================================================

    PlayListItemDraggerControl.h
    Created: 27 Dec 2023 4:52:45pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "DraggerControl.h"
#include "Engine/PlayList/PlayListItem.h"

class PlayListItemDraggerControl : public DraggerControl
{
public:
    
    PlayListItemDraggerControl(juce::Component* componentToDrag,
                               std::shared_ptr<PlayListItem> playListItem,
                               std::shared_ptr<ZoomHandler> zoomHandler,
                               juce::Colour colour,
                               std::shared_ptr<RegionSelector> regionSelector) :
        DraggerControl(componentToDrag, zoomHandler, colour, regionSelector),
        playListItem(playListItem)
    {
    }

    ~PlayListItemDraggerControl() override
    {
    }
    
    void setRegionDataInSeconds(const juce::Range<double> newRegionData) override
    {
    
//        const auto audioResources = audioSubGroup->getAudioResources();
//        if (audioResources.size() > 0)
//        {
//            const auto transportPositionInSeconds = audioResources[0]->getTransportPositionSeconds();
//            auto regionData = audioResources[0]->getRegionDataInSeconds();
//
//            switch (currentDragMode)
//            {
//                case leftEdge:
//                    {
//                        // offset in file
//                        auto diff = newRegionData.getStart() - transportPositionInSeconds;
//                        auto newLength = regionData.getLength() - diff;
//                        auto newStart = regionData.getStart() + diff;
//
//                        for (auto res : audioResources)
//                        {
//                            res->setRegionDataInSeconds(juce::Range<double>(newStart, newStart + newLength));
//                            res->setTransportPosition(newRegionData.getStart());
//                        }
//                        repaint();
//                    }
//                    break;
//                case rightEdge:
//                    {
//                        // duration
//                        regionData.setLength(newRegionData.getLength());
//                        for (auto res : audioResources)
//                        {
//                            res->setRegionDataInSeconds(regionData);
//                        }
//                    }
//                    break;
//                case middleEdge:
//                    // position in transport
//                    for (auto res : audioResources)
//                    {
//                        res->setTransportPosition(newRegionData.getStart());
//                    }
//                    break;
//                default:
//                    break;
//            }
//        }
    }
    
    
    bool isSelected() const override
    {
        return playListItem->isSelected();
    }
    
    void setSelected(bool bSelected, bool deselectOthers) override
    {
        playListItem->setSelected(bSelected);
    }
    
    const juce::String getLabelString() const override
    {
        return playListItem->getRegion()->getName();
    }
    
    bool validateData() override
    {
        return true;
//        const auto audioResources = audioSubGroup->getAudioResources();
//        bool result = false;
//        for (auto res : audioResources)
//        {
//            result |= res->validateData();
//        }
//        return result;
    }
    
    
private:
    std::shared_ptr<PlayListItem> playListItem;
};
