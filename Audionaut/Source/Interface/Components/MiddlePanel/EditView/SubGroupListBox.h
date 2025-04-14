//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Components/MiddlePanel/EditView/RegionEditComponent.h"

#include "Engine/AudiumEngine.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Group/ResourceGroup.h"

class ZoomHandler;
class RegionSelector;

class SubGroupListBox  : public audium::ListBox
{
public:
    SubGroupListBox(std::shared_ptr<audium::AudiumEngine> audiumEngine,
                    std::shared_ptr<audium::ResourceGroup> resourceGroup,
                    std::shared_ptr<ZoomHandler> zoomHandler,
                    std::shared_ptr<RegionSelector> regionSelector) :
        audium::ListBox("SubGroupListBox", nullptr),
        resourceGroup(resourceGroup)
    {
        // transparent backgroud:
        setColour(audium::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        
        
        // region edit component
        regionEditComponent.reset(new RegionEditComponent(audiumEngine,
                                                          resourceGroup,
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
    
    void updateFromEngine(std::shared_ptr<audium::ResourceGroup> resourceGroup)
    {
        resourceGroup = resourceGroup;
        regionEditComponent->updateFromEngine(resourceGroup);
        updateContent();
    }

private:
    std::shared_ptr<audium::ResourceGroup> resourceGroup;
    
    std::unique_ptr<RegionEditComponent> regionEditComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubGroupListBox)
};
