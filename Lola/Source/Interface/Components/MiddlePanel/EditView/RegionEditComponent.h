//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
        audiumEngine->getAudioTrackContainer()->sendActionMessage(updateSelection);
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
