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
#include "Engine/AudioRegionContainer.h"

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
                        std::shared_ptr<AudioResource> audioResource,
                        std::shared_ptr<ZoomHandler> zoomHandler,
                        std::shared_ptr<RegionSelector> regionSelector) :
        audiumEngine(audiumEngine),
        audioResource(audioResource),
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
        auto regions = audiumEngine->getAudioRegionContainer()->getRegionsForResource(audioResource);
        auto count = 0;
        for (auto region : regions)
        {
            if (count < regionEditControls.size())
            {
                auto regionEditControl = regionEditControls[count];
                regionEditControl->setBounds(0, DraggerControl::draggerHeight, 100, getHeight() - DraggerControl::draggerHeight);
                regionEditControl->updateFromEngine();
            }
            count++;
        }
    }
    
    void updateFromEngine()
    {
        if (mustRebuildComponents())
        {
            rebuildComponents();
            resized();
        }
        else
        {
            for (auto regionEdit : regionEditControls)
            {
                regionEdit->updateFromEngine();
            }
        }
    }

    bool mustRebuildComponents() const
    {
        auto regions = audiumEngine->getAudioRegionContainer()->getRegionsForResource(audioResource);
        if (regions.size() != regionEditControls.size())
        {
            return true;
        }
        
        return false;
    }

    void rebuildComponents()
    {
        std::cout << "RegionEditComponent::rebuildComponents" << std::endl;
        
        regionEditControls.clear();
        
        auto regions = audiumEngine->getAudioRegionContainer()->getRegionsForResource(audioResource);
        for (auto region : regions)
        {
            auto view = std::shared_ptr<RegionEditControl>(new RegionEditControl(region, zoomHandler, audiumEngine, regionSelector));
            addAndMakeVisible(view.get());
            regionEditControls.push_back(view);
        }
    }
    
private:
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioResource> audioResource;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<RegionSelector> regionSelector;
    juce::Colour colour;
    
    std::vector<std::shared_ptr<RegionEditControl>> regionEditControls;
    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionEditComponent)
};
