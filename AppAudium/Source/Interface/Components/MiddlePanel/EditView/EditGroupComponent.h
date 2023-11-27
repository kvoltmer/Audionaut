/*
  ==============================================================================

    EditGroupComponent.h
    Created: 27 Nov 2023 12:11:36pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MiddlePanel/GroupBaseComponent.h"
#include "Interface/Views/AudioResourceView.h"

//==============================================================================
/*
*/
class EditGroupComponent  : public GroupBaseComponent
{
public:
        
    EditGroupComponent (std::shared_ptr<AudioGroup> group,
                         std::shared_ptr<AudiumEngine> audiumEngine,
                         std::shared_ptr<ZoomHandler> zoomHandler) :
        GroupBaseComponent(group, audiumEngine, zoomHandler)
    {
        // this component doesn't handle mouse events
        setInterceptsMouseClicks(false, false);
        
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
    
    bool mustRebuildComponents() const { return true; }
    
    void rebuildComponents()
    {
        removeAllChildren();
        children.clear();
        
        // create views
        auto audioResources = audioGroup->getAudioResources();
        for (auto audioResource : audioResources)
        {
            auto view = std::shared_ptr<AudioResourceView>(new AudioResourceView(audioResource, zoomHandler, nullptr, audioGroup->getColour()));
            
            addAndMakeVisible(view.get());
            children.push_back(view);
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
            if (count < children.size())
            {
                auto child = children[count];
                if (child != nullptr)
                {
                    auto width = zoomHandler->secondsToX(audioResource->getLengthInSeconds());
                    child->setBounds(0, top, width, audioResource->getHeight());
                }
                count++;
            }
            top += height;
        }
    }

private:
    
    std::vector<std::shared_ptr<AudioResourceView>> children;

    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditGroupComponent)
};
