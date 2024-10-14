/*
  ==============================================================================

    RegionEditComponent.h
    Created: 9 Dec 2023 3:17:46pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Group/AudioSubGroup.h"

#include "Interface/Controls/RegionEditControl.h"
#include "Interface/Controls/DraggerControl.h"

//==============================================================================
/*
Contains all RegionEditControls
*/
class RegionEditComponent  : public juce::Component
{
public:
    RegionEditComponent(std::shared_ptr<AudiumEngine> audiumEngine,
                        std::shared_ptr<AudioSubGroup> audioSubGroup,
                        std::shared_ptr<ZoomHandler> zoomHandler,
                        std::shared_ptr<RegionSelector> regionSelector) :
        audiumEngine(audiumEngine),
        audioSubGroup(audioSubGroup),
        zoomHandler(zoomHandler),
        regionSelector(regionSelector)
    {
        rebuildComponents();
    }

    ~RegionEditComponent() override
    {
    }
    
    void resized() override
    {
        const auto regions = audioSubGroup->getAudioRegions();
        auto count = 0;
        for (auto region : regions)
        {
            if (count < regionEditControls.size())
            {
                auto regionEditControl = regionEditControls[count];
                regionEditControl->setBounds(0, DraggerControl::draggerHeight, 100, getHeight() - DraggerControl::draggerHeight);
                regionEditControl->updateFromEngine(region);
            }
            count++;
        }
    }
    
    void updateFromEngine(std::shared_ptr<AudioSubGroup> subGroup)
    {
        //bool rebuild = false;
        if (audioSubGroup != subGroup)
        {
            //rebuild = true;
            audioSubGroup = subGroup;
        }
        
        if (mustRebuildComponents())
        {
            rebuildComponents();
            resized();
        }
        else
        {
            const auto regions = audioSubGroup->getAudioRegions();
            jassert(regions.size() == regionEditControls.size());
            auto count = 0;
            for (auto regionEdit : regionEditControls)
            {
                regionEdit->updateFromEngine(regions[count]);
                count++;
            }
        }
    }

    bool mustRebuildComponents() const
    {
        const auto regions = audioSubGroup->getAudioRegions();
        if (regions.size() != regionEditControls.size())
        {
            return true;
        }
        
        return false;
    }

    void rebuildComponents()
    {
        std::cout << "RegionEditComponent::rebuildComponents" << std::endl;
        
        removeAllChildren();
        regionEditControls.clear();
        
        const auto regions = audioSubGroup->getAudioRegions();
        for (auto region : regions)
        {
            auto view = std::shared_ptr<RegionEditControl>(new RegionEditControl(region, zoomHandler, audiumEngine, regionSelector));
            addAndMakeVisible(view.get());
            regionEditControls.push_back(view);
        }
    }
    
    void mouseDown (const juce::MouseEvent& e) override
    {
        audiumEngine->getAudioTrackContainer()->getAudioRegionAdapter().deselectAll();
        audiumEngine->getAudioTrackContainer()->sendActionMessage(regionSelectedAction);
    }
    
private:
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioSubGroup> audioSubGroup;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<RegionSelector> regionSelector;
    juce::Colour colour;
    
    std::vector<std::shared_ptr<RegionEditControl>> regionEditControls;
    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionEditComponent)
};
