/*
  ==============================================================================

    ArrangementGroupComponent.cpp
    Created: 27 Nov 2022 3:25:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "ArrangementGroupComponent.h"
#include "Util/EngineAccess.h"
#include "Interface/Controls/AudioGroupListBox.h"
#include "Interface/Components/MiddlePanel/ArrangementView/PlayListItemComponent.h"
#include "Interface/ColourIds.h"
#include "Interface/Views/AudioRegionView.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"


using namespace audium;




void ArrangementGroupComponent::refreshComponent (std::shared_ptr<AudioGroup> group, bool forceRebuildComponents)
{
    audioGroup = group;
    
    if (mustRebuildComponents() ||
        forceRebuildComponents)
    {
        rebuildComponents();
    }
    resized();
}

bool ArrangementGroupComponent::mustRebuildComponents() const
{
    // compare play list items
    auto playListContainer = audiumEngine->getPlayListContainer(audioGroup);
    auto playListItems = playListContainer->getPlayListItems();
    
    if (playListItems.size() != playListItemComponents.size())
    {
        return true;
    }
    
    for (auto i = 0; i < playListItems.size(); i++)
    {
        if (playListItems[i] != playListItemComponents[i]->getPlayListItem())
        {
            return true;
        }
    }
    
    return false;
}

void ArrangementGroupComponent::rebuildComponents()
{
    removeAllChildren();
    playListItemComponents.clear();
    
    // get all play list items and create components
    auto playListContainer = audiumEngine->getPlayListContainer(audioGroup);
    jassert(playListContainer);
    auto playListItems = playListContainer->getPlayListItems();
    
    for (auto playListItem : playListItems)
    {
        auto groupRegion = std::shared_ptr<PlayListItemComponent>(new PlayListItemComponent(audiumEngine, audioGroup, playListItem, zoomHandler, regionSelector));
        
        addAndMakeVisible(groupRegion.get());
        playListItemComponents.push_back(groupRegion);
    }
}


void ArrangementGroupComponent::resized()
{
    for (auto regionView : playListItemComponents)
    {
        auto playListItem = regionView->getPlayListItem();
        auto start = zoomHandler->clocksToX(playListItem->getAbsolueStartTime(audium::clocks));
        auto width = zoomHandler->clocksToX(playListItem->getDurationTime(audium::clocks));
        juce::Rectangle<double> rect_tmp(start, getLocalBounds().getY(), width, getLocalBounds().getHeight());
        
        regionView->setBounds(rect_tmp.toNearestInt());
    }
}



