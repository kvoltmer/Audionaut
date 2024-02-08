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
class AudioSubGroup;

class SubGroupListBox  : public audium::ListBox
{
public:
    SubGroupListBox(std::shared_ptr<AudiumEngine> audiumEngine,
                    std::shared_ptr<AudioSubGroup> audioSubGroup,
                    std::shared_ptr<ZoomHandler> zoomHandler,
                    std::shared_ptr<RegionSelector> regionSelector) :
        audium::ListBox("SubGroupListBox", nullptr),
        audioSubGroup(audioSubGroup)
    {
        // transparent backgroud:
        setColour(audium::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        
        
        // region edit component
        regionEditComponent.reset(new RegionEditComponent(audiumEngine,
                                                          audioSubGroup,
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
    
    void updateFromEngine(std::shared_ptr<AudioSubGroup> audioSubGroup)
    {
        regionEditComponent->updateFromEngine(audioSubGroup);
        updateContent();
    }

private:
    std::shared_ptr<AudioSubGroup> audioSubGroup;
    
    std::unique_ptr<RegionEditComponent> regionEditComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubGroupListBox)
};
