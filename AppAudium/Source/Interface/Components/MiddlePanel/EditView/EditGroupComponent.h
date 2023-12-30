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
#include "Interface/Controls/SubGroupDraggerControl.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Models/SubGroupListBoxModel.h"
#include "SubGroupListBox.h"

//==============================================================================
/*
 
Display AudioSubGroups -> Timeline
 
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
        if (audioGroup->getAudioSubGroups().size() != subGroupListViews.size())
        {
            return true;
        }
        
        return false;
    }
    
    void rebuildComponents()
    {
        std::cout << "EditGroupComponent::rebuildComponents" << std::endl;
        
        // cleanup
        removeAllChildren();
        subGroupListViews.clear();
        subGroupListModels.clear();
        
        // create a ListBox and ListBoxModel for each sub group
        auto subGroups = audioGroup->getAudioSubGroups();
        for (auto subGroup : subGroups)
        {
            auto resources = subGroup->getAudioResources();
            if (resources.size() > 0)
            {
                auto subGroupListView = std::shared_ptr<SubGroupListBox>(new SubGroupListBox(audiumEngine,
                                                                                             resources[0],
                                                                                             zoomHandler,
                                                                                             regionSelector));
                
                auto subGroupListBoxModel = std::shared_ptr<SubGroupListBoxModel>(new SubGroupListBoxModel(subGroupListView,
                                                                                                           subGroup,
                                                                                                           audiumEngine,
                                                                                                           zoomHandler,
                                                                                                           regionSelector));
                subGroupListModels.push_back(subGroupListBoxModel);
                subGroupListView->setModel(subGroupListBoxModel.get());
                
                // create dragger as header of ListBox
                auto dragger = std::unique_ptr<SubGroupDraggerControl>(new SubGroupDraggerControl(subGroupListView.get(),
                                                                                  subGroup,
                                                                                  zoomHandler,
                                                                                  audioGroup->getColour(),
                                                                                  regionSelector));
                dragger->addChangeListener(this);
                subGroupListView->setHeaderComponent(std::move(dragger));

                
                subGroupListView->getHeaderComponent()->setSize(getWidth(), DraggerControl::draggerHeight);
                subGroupListView->setOutlineThickness(0);
                
                addAndMakeVisible(subGroupListView.get());
                
                subGroupListViews.push_back(subGroupListView);
            }
        }
    }
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        updateFromEngine();
        resized();
    }
    
    void updateFromEngine()
    {
        for (auto subGroupListView : subGroupListViews)
        {
            subGroupListView->updateFromEngine();
            subGroupListView->updateContent();
        }
    }

    void resized() override
    {
        
        // set size and position of subgroups on timeline
        auto subGroups = audioGroup->getAudioSubGroups();
        jassert(subGroups.size() == subGroupListViews.size());
        int counter = 0;
        for (auto subGroup : subGroups)
        {
            
            auto pos = 0.0;
            auto width = 0.0;
            auto resources = subGroup->getAudioResources();
            for (auto resource : resources)
            {
                pos = zoomHandler->secondsToX(resource->getTransportPosition(audium::seconds));
                width = std::max(width, zoomHandler->secondsToX(resource->getRegionData(audium::seconds).getLength()));
            }
            
            auto height = audioGroup->getTotalHeight() + DraggerControl::draggerHeight;
            
            juce::Rectangle<double> rect_tmp(pos, 0.0, width, height);
            
            if (counter < subGroupListViews.size())
                subGroupListViews[counter]->setBounds(rect_tmp.toNearestInt());
            counter++;
        }
    }

private:
        
    std::vector<std::shared_ptr<SubGroupListBox>> subGroupListViews;
    std::vector<std::shared_ptr<SubGroupListBoxModel>> subGroupListModels;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditGroupComponent)
};
