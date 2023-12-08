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
#include "Interface/Controls/DraggerControl.h"
#include "Interface/Handlers/ZoomHandler.h"

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
        
        return false;
    }
    
    void rebuildComponents()
    {
        std::cout << "EditGroupComponent::rebuildComponents" << std::endl;
        removeAllChildren();
        audioResourceViews.clear();
        draggerControls.clear();
        
        // create views
        auto audioResources = audioGroup->getAudioResources();
        for (auto audioResource : audioResources)
        {
            auto view = std::shared_ptr<AudioResourceView>(new AudioResourceView(audiumEngine,
                                                                                 audioResource,
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
            addAndMakeVisible(dragger.get());
            draggerControls.push_back(dragger);
            dragger->addChangeListener(this);
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
                dragger->setSize(resourceView->getWidth(), DraggerControl::draggerHeight);
                dragger->setTopLeftPosition(pos, top);
                dragger->updateFromEngine();
                
                count++;
            }
        }
    }

    
    
private:
    
    std::vector<std::shared_ptr<AudioResourceView>> audioResourceViews;
    std::vector<std::shared_ptr<DraggerControl>> draggerControls;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditGroupComponent)
};
