/*
  ==============================================================================

    EditGroupComponent.h
    Created: 27 Nov 2023 12:11:36pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/AudioRegionContainer.h"

#include "Interface/Components/MiddlePanel/GroupBaseComponent.h"
#include "Interface/Views/AudioResourceView.h"
#include "Interface/Controls/RegionEditControl.h"
#include "Interface/Handlers/ZoomHandler.h"

//==============================================================================
/*
*/
class EditGroupComponent  : public GroupBaseComponent
{
public:
        
    EditGroupComponent (std::shared_ptr<AudioGroup> group,
                        std::shared_ptr<AudiumEngine> audiumEngine,
                        std::shared_ptr<ZoomHandler> zoomHandler,
                        std::shared_ptr<RegionSelector> regionSelector) :
        GroupBaseComponent(group, audiumEngine, zoomHandler, regionSelector)
    {
        // this component doesn't handle mouse events
        //setInterceptsMouseClicks(false, false);
        
        refreshComponent(group);
    }    
    
    void refreshComponent (std::shared_ptr<AudioGroup> group, bool forceRebuildComponents = false) override
    {
        audioGroup = group;
        
        if (mustRebuildComponents() ||
            forceRebuildComponents)
        {
            rebuildComponents();
        }
        resized();
    }
    
    bool mustRebuildComponents() const
    {
        auto audioResources = audioGroup->getAudioResources();
        if (audioResources.size() != audioResourceViews.size())
        {
            return true;
        }
        
        auto regions = audiumEngine->getAudioRegionContainer()->getRegionsForGroup(audioGroup);
        if (regions.size() != regionEditControls.size())
        {
            return true;
        }
        
        return false;
    }
    
    void rebuildComponents()
    {
        std::cout << "EditGroupComponent::rebuildComponents" << std::endl;
        removeAllChildren();
        audioResourceViews.clear();
        regionEditControls.clear();
        
        // create views
        auto audioResources = audioGroup->getAudioResources();
        for (auto audioResource : audioResources)
        {
            auto view = std::shared_ptr<AudioResourceView>(new AudioResourceView(audioResource, zoomHandler, nullptr, audioGroup->getColour()));
            addAndMakeVisible(view.get());
            audioResourceViews.push_back(view);
        }
        
        auto regions = audiumEngine->getAudioRegionContainer()->getRegionsForGroup(audioGroup);
        for (auto region : regions)
        {
            auto view = std::shared_ptr<RegionEditControl>(new RegionEditControl(region, zoomHandler, audiumEngine, regionSelector));
            addAndMakeVisible(view.get());
            regionEditControls.push_back(view);
        }
        
        
    }

    void resized() override
    {
        int top = 0;
        int count = 0;
        auto audioResources = audioGroup->getAudioResources();
        for (auto audioResource : audioResources)
        {
            auto height = audioResource->getHeight();
            if (count < audioResourceViews.size())
            {
                auto child = audioResourceViews[count];
                if (child != nullptr)
                {
                    auto width = zoomHandler->secondsToX(audioResource->getLengthInSeconds());
                    child->setBounds(0, top, width, audioResource->getHeight());
                }
                count++;
            }
            top += height;
        }
        
        auto regions = audiumEngine->getAudioRegionContainer()->getRegionsForGroup(audioGroup);
        count = 0;
        for (auto region : regions)
        {
            if (count < regionEditControls.size())
            {
                auto control = regionEditControls[count];
                jassert(control != nullptr);
                auto start = region->getRegionDataInSeconds().getStart();
                auto length = region->getRegionDataInSeconds().getLength();
                
                auto startX = zoomHandler->secondsToX(start);
                auto width = zoomHandler->secondsToX(length);
                control->setBounds(startX, 0, width, getHeight());
            }
            count++;
        }
    }

private:
    
    std::vector<std::shared_ptr<AudioResourceView>> audioResourceViews;
    
    std::vector<std::shared_ptr<RegionEditControl>> regionEditControls;

    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditGroupComponent)
};
