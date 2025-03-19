//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

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
    RegionEditComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine,
                        std::shared_ptr<audium::AudioSubGroup> audioSubGroup,
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
        const auto regions = audioSubGroup->getAudioRegionContainer()->getObjects();
        auto count = 0;
        for (auto region : regions)
        {
            if (count < regionEditControls.size())
            {
                auto regionEditControl = regionEditControls[count];
                regionEditControl->setBounds(0, DraggerControl::draggerHeight, 100, getHeight() - DraggerControl::draggerHeight);
                regionEditControl->updateFromEngine(region);
                if (region->isSelected())
                    regionEditControl->toFront(false);
            }
            count++;
        }
    }
    
    void updateFromEngine(std::shared_ptr<audium::AudioSubGroup> subGroup)
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
            const auto regions = audioSubGroup->getAudioRegionContainer()->getObjects();
            jassert(regions.size() == regionEditControls.size());
            auto count = 0;
            for (auto regionEditControl : regionEditControls)
            {
                auto region = regions[count++];
                regionEditControl->updateFromEngine(region);
                if (region->isSelected())
                    regionEditControl->toFront(false);
            }
        }
        
        regionSelector->updateFromEngine();

    }

    bool mustRebuildComponents() const
    {
        const auto regions = audioSubGroup->getAudioRegionContainer()->getObjects();
        if (regions.size() != regionEditControls.size())
        {
            return true;
        }
        
        return false;
    }

    void rebuildComponents()
    {
        //std::cout << "RegionEditComponent::rebuildComponents" << std::endl;
        
        removeAllChildren();
        regionEditControls.clear();
        
        const auto regions = audioSubGroup->getAudioRegionContainer()->getObjects();
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
        audiumEngine->getAudioTrackContainer()->sendActionMessage(audium::updateSelection);
    }
    
private:
    
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<audium::AudioSubGroup> audioSubGroup;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<RegionSelector> regionSelector;
    juce::Colour colour;
    
    std::vector<std::shared_ptr<RegionEditControl>> regionEditControls;
    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionEditComponent)
};
