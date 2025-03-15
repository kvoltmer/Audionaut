/*
  ==============================================================================

    AudioTrackRegionEditComponent.h
    Created: 27 Nov 2023 12:11:36pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/Region/AudioRegionContainer.h"

#include "Interface/Components/MiddlePanel/AudioTrackBaseComponent.h"
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

class AudioTrackRegionEditComponent  : public AudioTrackBaseComponent, public juce::ChangeListener
{
public:
        
    AudioTrackRegionEditComponent (std::shared_ptr<AudioTrack> track,
                        std::shared_ptr<AudiumEngine> audiumEngine,
                        std::shared_ptr<ZoomHandler> zoomHandler,
                        std::shared_ptr<RegionSelector> regionSelector) :
        AudioTrackBaseComponent(track, audiumEngine, zoomHandler, regionSelector)
    {
        refreshComponent(track);
    }    
    
    void refreshComponent (std::shared_ptr<AudioTrack> track, bool forceRebuildComponents = false) override
    {
        audioTrack = track;
        
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
        if (audioTrack->getAudioSubGroups().size() != subGroupListViews.size())
        {
            return true;
        }
        
        return false;
    }
    
    void rebuildComponents()
    {
        //std::cout << "AudioTrackRegionEditComponent::rebuildComponents" << std::endl;
        
        // cleanup
        removeAllChildren();
        subGroupListViews.clear();
        subGroupListModels.clear();
        
        // create a ListBox and ListBoxModel for each sub track
        auto subGroups = audioTrack->getAudioSubGroups();
        for (auto subGroup : subGroups)
        {
            auto subGroupListView = std::shared_ptr<SubGroupListBox>(new SubGroupListBox(audiumEngine,
                                                                                         subGroup,
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
            auto dragger = std::unique_ptr<SubGroupDraggerControl>(new SubGroupDraggerControl(audiumEngine,
                                                                                              subGroup,
                                                                                              zoomHandler,
                                                                                              audioTrack->getColour(),
                                                                                              regionSelector));
            dragger->setComponentToDrag(subGroupListView.get());
            dragger->setPositionableObject(subGroup);
            dragger->addChangeListener(this);
            subGroupListView->setHeaderComponent(std::move(dragger));

            
            subGroupListView->getHeaderComponent()->setSize(getWidth(), DraggerControl::draggerHeight);
            subGroupListView->setOutlineThickness(0);
            
            addAndMakeVisible(subGroupListView.get());
            
            subGroupListViews.push_back(subGroupListView);
            
        }
    }
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        audiumEngine->getAudioResourceContainer()->sendActionMessage("");
    }
    
    void updateFromEngine()
    {
        const auto subGroups = audioTrack->getAudioSubGroups();
        jassert(subGroups.size() == subGroupListViews.size());
        int counter = 0;
        for (auto subGroup : subGroups)
        {
            subGroupListModels[counter]->setAudioSubGroup(subGroup);
            subGroupListViews[counter]->updateFromEngine(subGroup);
            counter++;
        }
    }

    void resized() override
    {
        
        // set size and position of subgroups on timeline
        auto subGroups = audioTrack->getAudioSubGroups();
        //jassert(subGroups.size() == subGroupListViews.size());
        int counter = 0;
        for (auto subGroup : subGroups)
        {
            auto posRange = subGroup->getAbsolutePositionRange(audium::clocks);
            auto pos = zoomHandler->clocksToX(posRange.getStart());
            auto width = zoomHandler->clocksToX(posRange.getLength());
            
            auto height = audioTrack->getTotalHeight() + DraggerControl::draggerHeight;
            
            juce::Rectangle<double> rect_tmp(pos, 0.0, width, height);
            
            if (counter < subGroupListViews.size())
                subGroupListViews[counter]->setBounds(rect_tmp.toNearestInt());
            counter++;
        }
    }

private:
        
    std::vector<std::shared_ptr<SubGroupListBox>> subGroupListViews;
    std::vector<std::shared_ptr<SubGroupListBoxModel>> subGroupListModels;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrackRegionEditComponent)
};
