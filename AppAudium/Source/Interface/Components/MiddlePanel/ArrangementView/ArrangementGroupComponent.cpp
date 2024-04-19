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
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Interface/Handlers/SnapToGridHandler.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Interface/Controls/RegionLabel.h"

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
        auto groupRegion = std::shared_ptr<PlayListItemComponent>(new PlayListItemComponent(audiumEngine,
                                                                                            audioGroup,
                                                                                            playListContainer,
                                                                                            playListItem,
                                                                                            zoomHandler,
                                                                                            regionSelector));
        
        addAndMakeVisible(groupRegion.get());
        playListItemComponents.push_back(groupRegion);
    }
}

void ArrangementGroupComponent::resized()
{
    for (auto regionView : playListItemComponents)
    {
        auto playListItem = regionView->getPlayListItem();
        auto start = zoomHandler->clocksToX(playListItem->getAbsolutePosition(audium::clocks));
        auto width = zoomHandler->clocksToX(playListItem->getDurationTime(audium::clocks));
        juce::Rectangle<double> rect_tmp(start, getLocalBounds().getY(), width, getLocalBounds().getHeight());
        
        regionView->setBounds(rect_tmp.toNearestInt());
    }
}

void ArrangementGroupComponent::itemDragMove (const SourceDetails &dragSourceDetails)
{
    auto x = dragSourceDetails.localPosition.x;
    auto start = zoomHandler->xToClocks(x);
    auto end = start + 0.01;
    Range<double> rangeInClocks(start, end);
    
    zoomHandler->getSnapToGridHandler()->publishRange(rangeInClocks);
}

void ArrangementGroupComponent::itemDragExit (const SourceDetails &dragSourceDetails)
{
    zoomHandler->getSnapToGridHandler()->clearRange();
    repaint();
}

void ArrangementGroupComponent::itemDropped (const SourceDetails &dragSourceDetails)
{

    if ( RegionLabel* regionLabel = dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()))
    {
        // undo action stores current state
        auto action = std::make_unique<audium::UndoableContainerAction>(audioGroup->getAudioGroupContainer());
                
        auto insertIndex = 0;
        
        auto x = dragSourceDetails.localPosition.x;
        auto clocks = zoomHandler->xToClocks(x);
        Range<double> rangeInClocks(0.0, clocks);

        auto items = audioGroup->getPlayListContainer()->itemsAtAbsoluteRange(rangeInClocks, audium::clocks);
        if (items.size() > 0)
        {
            insertIndex = audioGroup->getPlayListContainer()->getPlayListItemIndex(items.back()) + 1;
            std::cout << "insert index: " << insertIndex << std::endl;
        }
        
        if (auto playListItem = audioGroup->getPlayListContainer()->createPlayListItemUI(regionLabel->getRowNumber(), insertIndex))
        {
            zoomHandler->snapToGrid(clocks);
            playListItem->setAbsolutePosition(clocks, audium::clocks);
            
            action->storeNewState();
            auto undoManager = audioGroup->getAudioGroupContainer().getUndoManager();
            undoManager->perform(action.release(), "Playlist modified");
            undoManager->beginNewTransaction();
        }
    }
    
    zoomHandler->getSnapToGridHandler()->clearRange();
    repaint();
}
