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
#include "Interface/Controls/DraggerControl.h"
#include "Interface/Handlers/ZoomHandler.h"

//==============================================================================
/*
*/
class EditGroupComponent  : public GroupBaseComponent, public juce::ChangeListener
{
public:
        
    EditGroupComponent (std::shared_ptr<AudioGroup> group,
                        std::shared_ptr<AudiumEngine> audiumEngine,
                        std::shared_ptr<ZoomHandler> zoomHandler,
                        std::shared_ptr<RegionSelector> regionSelector) :
        GroupBaseComponent(group, audiumEngine, zoomHandler, regionSelector)
    {
        refreshComponent(group);
    }    
    
    void refreshComponent (std::shared_ptr<AudioGroup> group, bool forceRebuildComponents = false) override
    {
        audioGroup = group;
        
        if (mustRebuildComponents() ||
            forceRebuildComponents)
        {
            rebuildComponents();
            resized();
        }
        else
        {
            resized();
            updateFromEngine();
        }
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
        draggerControls.clear();
        regionEditControls.clear();
        
        // create views
        auto audioResources = audioGroup->getAudioResources();
        for (auto audioResource : audioResources)
        {
            auto view = std::shared_ptr<AudioResourceView>(new AudioResourceView(audioResource,
                                                                                 zoomHandler,
                                                                                 nullptr,
                                                                                 audioGroup->getColour(),
                                                                                 regionSelector));
            addAndMakeVisible(view.get());
            audioResourceViews.push_back(view);
            
            auto dragger = std::shared_ptr<DraggerControl>(new DraggerControl(audioResource,
                                                                              zoomHandler,
                                                                              nullptr,
                                                                              audioGroup->getColour(),
                                                                              regionSelector));
            dragger->addChangeListener(this);
            addAndMakeVisible(dragger.get());
            draggerControls.push_back(dragger);
        }
        
        auto regions = audiumEngine->getAudioRegionContainer()->getRegionsForGroup(audioGroup);
        for (auto region : regions)
        {
            auto view = std::shared_ptr<RegionEditControl>(new RegionEditControl(region, zoomHandler, audiumEngine, regionSelector));
            addAndMakeVisible(view.get());
            regionEditControls.push_back(view);
        }
    }
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        updateFromEngine();
    }
    
    void updateFromEngine()
    {
        for (auto resourceView : audioResourceViews)
        {
            resourceView->updateFromEngine();
        }
        
        for (auto draggerControl : draggerControls)
        {
            draggerControl->updateFromEngine();
        }
        
        for (auto regionEdit : regionEditControls)
        {
            regionEdit->updateFromEngine();
        }
    }

    void resized() override
    {
        int count = 0;
        auto audioResources = audioGroup->getAudioResources();
        for (auto audioResource : audioResources)
        {
            auto pos = zoomHandler->secondsToX(audioResource->getTransportPosition());
            auto top = audioResource->getTop();
            if (count < audioResourceViews.size())
            {
                auto resourceView = audioResourceViews[count];
                
                resourceView->setTopLeftPosition(pos, top);
                resourceView->updateFromEngine();
                
                auto dragger = draggerControls[count];
                dragger->setSize(resourceView->getWidth(), draggerHeight);
                dragger->setTopLeftPosition(pos, top);
                dragger->updateFromEngine();
                
                count++;
            }
        }
        
        auto regions = audiumEngine->getAudioRegionContainer()->getRegionsForGroup(audioGroup);
        count = 0;
        for (auto region : regions)
        {
            if (count < regionEditControls.size())
            {
                auto regionEditControl = regionEditControls[count];
                regionEditControl->setBounds(0, 0, 100, getHeight());
                regionEditControl->updateFromEngine();
            }
            count++;
        }
    }

    static constexpr int draggerHeight = 19;
    
private:
    
    std::vector<std::shared_ptr<AudioResourceView>> audioResourceViews;
    std::vector<std::shared_ptr<DraggerControl>> draggerControls;
    std::vector<std::shared_ptr<RegionEditControl>> regionEditControls;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditGroupComponent)
};
