//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Interface/Controls/DraggerControl.h"

#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioClip.h"

class SubGroupDraggerControl : public DraggerControl
{
public:
    
    SubGroupDraggerControl(std::shared_ptr<audium::AudiumEngine> audiumEngine,
                           std::shared_ptr<audium::ResourceGroup> resourceGroup,
                           std::shared_ptr<ZoomHandler> zoomHandler,
                           juce::Colour colour,
                           std::shared_ptr<RegionSelector> regionSelector) :
        DraggerControl(audiumEngine,
                       zoomHandler,
                       colour,
                       regionSelector),
        resourceGroup(resourceGroup)
    {
        regionSelector->subGroupDraggerControls.push_back(this);
    }

    ~SubGroupDraggerControl() override
    {
        std::erase_if(regionSelector->subGroupDraggerControls, [this](const auto* item) {
            return item == this;
        });
    }
        
    bool isSelected() const override
    {
        return resourceGroup->isSelected();
    }
    
    void setSelected(bool bSelected, bool deselectOthers) override
    {
        if (deselectOthers)
            resourceGroup->getAudioTrack().getSelectionManager()->deselectAll();
        resourceGroup->setSelected(bSelected, false);
    }
    
    void shiftSelect() override;
    
    const juce::String getLabelString() const override
    {
        return resourceGroup->getName();
    }
    
    bool validateData() override;
        
private:
    std::shared_ptr<audium::ResourceGroup> resourceGroup;
};
