/*
  ==============================================================================

    SubGroupDraggerControl.h
    Created: 27 Dec 2023 4:52:45pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "DraggerControl.h"

class SubGroupDraggerControl : public DraggerControl
{
public:
    
    SubGroupDraggerControl(juce::Component* componentToDrag,
                   std::shared_ptr<AudioSubGroup> audioSubGroup,
                   std::shared_ptr<ZoomHandler> zoomHandler,
                   juce::Colour colour,
                   std::shared_ptr<RegionSelector> regionSelector) :
        DraggerControl(componentToDrag, zoomHandler, colour, regionSelector),
        audioSubGroup(audioSubGroup)
    {
    }

    ~SubGroupDraggerControl() override
    {
    }
    
    void setRegionDataInSeconds(const juce::Range<double> newRegionData) override
    {
        const auto audioResources = audioSubGroup->getAudioResources();
        if (audioResources.size() > 0)
        {
            const auto transportPositionInSeconds = audioResources[0]->getTransportPositionSeconds();
            auto regionData = audioResources[0]->getRegionDataInSeconds();
            
            switch (currentDragMode)
            {
                case leftEdge:
                    {
                        // offset in file
                        auto diff = newRegionData.getStart() - transportPositionInSeconds;
                        auto newLength = regionData.getLength() - diff;
                        auto newStart = regionData.getStart() + diff;
                        
                        for (auto res : audioResources)
                        {
                            res->setRegionDataInSeconds(juce::Range<double>(newStart, newStart + newLength));
                            res->setTransportPosition(newRegionData.getStart());
                        }
                        repaint();
                    }
                    break;
                case rightEdge:
                    {
                        // duration
                        regionData.setLength(newRegionData.getLength());
                        for (auto res : audioResources)
                        {
                            res->setRegionDataInSeconds(regionData);
                        }
                    }
                    break;
                case middleEdge:
                    // position in transport
                    for (auto res : audioResources)
                    {
                        res->setTransportPosition(newRegionData.getStart());
                    }
                    break;
                default:
                    break;
            }
        }
    }
    
    
    bool isSelected() const override
    {
        return audioSubGroup->isSelected();
    }
    
    void setSelected(bool bSelected, bool deselectOthers) override
    {
        audioSubGroup->setSelected(bSelected);
    }
    
    const juce::String getLabelString() const override
    {
        const auto audioResources = audioSubGroup->getAudioResources();
        if (audioResources.size() > 0)
        {
            return audioResources[0]->getFileNameWithoutExtension();
        }
        return "";
    }
    
    bool validateData() override
    {
        const auto audioResources = audioSubGroup->getAudioResources();
        bool result = false;
        for (auto res : audioResources)
        {
            result |= res->validateData();
        }
        return result;
    }
    
    
private:
    std::shared_ptr<AudioSubGroup> audioSubGroup;
};
