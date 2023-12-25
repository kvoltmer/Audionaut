/*
  ==============================================================================

    SubGroupListBox.h
    Created: 23 Dec 2023 12:24:41pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Components/MiddlePanel/EditView/RegionEditComponent.h"


class AudiumEngine;
class AudioResource;
class ZoomHandler;
class RegionSelector;

class SubGroupListBox  : public audium::ListBox
{
public:
    SubGroupListBox(std::shared_ptr<AudiumEngine> audiumEngine,
                    std::shared_ptr<AudioResource> audioResource,
                    std::shared_ptr<ZoomHandler> zoomHandler,
                    std::shared_ptr<RegionSelector> regionSelector) :
        audium::ListBox("SubGroupListBox", nullptr)
    {
        // transparent backgroud:
        setColour(audium::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        
        
        // region edit component
        regionEditComponent.reset(new RegionEditComponent(audiumEngine,
                                                          audioResource,
                                                          zoomHandler,
                                                          regionSelector));
        addAndMakeVisible(regionEditComponent.get());
    }

    ~SubGroupListBox() override
    {
    }

    void resized() override
    {
        audium::ListBox::resized();
        regionEditComponent->setBounds(getLocalBounds());
    }
    
    void updateFromEngine()
    {
        regionEditComponent->updateFromEngine();
    }

private:
    
    std::unique_ptr<RegionEditComponent> regionEditComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubGroupListBox)
};
