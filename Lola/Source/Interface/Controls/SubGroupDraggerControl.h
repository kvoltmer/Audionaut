/*
  ==============================================================================

    SubGroupDraggerControl.h
    Created: 27 Dec 2023 4:52:45pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "Interface/Controls/DraggerControl.h"

#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioClip.h"

class SubGroupDraggerControl : public DraggerControl
{
public:
    
    SubGroupDraggerControl(juce::Component* componentToDrag,
                           std::shared_ptr<AudiumEngine> audiumEngine,
                           std::shared_ptr<AudioSubGroup> audioSubGroup,
                           std::shared_ptr<ZoomHandler> zoomHandler,
                           juce::Colour colour,
                           std::shared_ptr<RegionSelector> regionSelector) :
        DraggerControl(componentToDrag, audiumEngine, zoomHandler, colour, regionSelector),
        audioSubGroup(audioSubGroup)
    {
    }

    ~SubGroupDraggerControl() override
    {
    }
    
    void commitData(const juce::Range<double> newData, audium::TimeContextType context) override;
    
    bool isSelected() const override
    {
        return audioSubGroup->isSelected();
    }
    
    void setSelected(bool bSelected, bool deselectOthers) override
    {
        if (deselectOthers)
            audioSubGroup->getAudioTrack().getSelectionManager()->deselectAll();
        audioSubGroup->setSelected(bSelected, false);
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
    
    bool validateData() override;
    
private:
    std::shared_ptr<AudioSubGroup> audioSubGroup;
};
