/*
  ==============================================================================

    AudioTrackComponent.cpp
    Created: 27 Nov 2022 3:25:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "AudioTrackComponent.h"
#include "Util/EngineAccess.h"
#include "Interface/Controls/AudioTrackListBox.h"
#include "Interface/Components/MiddlePanel/ArrangementView/PlayListItemComponent.h"
#include "Interface/ColourIds.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Interface/Handlers/SnapToGridHandler.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Interface/Controls/RegionLabel.h"
#include "Interface/Controls/DraggerControl.h"
#include "Interface/Models/PlayListTableListBoxItem.h"

using namespace audium;

void AudioTrackComponent::refreshComponent (std::shared_ptr<AudioTrack> track, bool forceRebuildComponents)
{
    audioTrack = track;
    
    if (mustRebuildComponents() ||
        forceRebuildComponents)
    {
        rebuildComponents();
    }
    resized();
}

bool AudioTrackComponent::mustRebuildComponents() const
{
    // compare play list items
    auto playListContainer = audiumEngine->getPlayListContainer(audioTrack);
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

void AudioTrackComponent::rebuildComponents()
{
    removeAllChildren();
    playListItemComponents.clear();
    
    // get all play list items and create components
    auto playListContainer = audiumEngine->getPlayListContainer(audioTrack);
    jassert(playListContainer);
    auto playListItems = playListContainer->getPlayListItems();
    
    for (auto playListItem : playListItems)
    {
        auto groupRegion = std::shared_ptr<PlayListItemComponent>(new PlayListItemComponent(audiumEngine,
                                                                                            audioTrack,
                                                                                            playListContainer,
                                                                                            playListItem,
                                                                                            zoomHandler,
                                                                                            regionSelector));
        
        addAndMakeVisible(groupRegion.get());
        playListItemComponents.push_back(groupRegion);
    }
}

void AudioTrackComponent::resized()
{
    for (auto regionView : playListItemComponents)
    {
        auto playListItem = regionView->getPlayListItem();
        auto start = zoomHandler->clocksToX(playListItem->getAbsolutePosition(audium::clocks));
        auto width = zoomHandler->clocksToX(playListItem->getDurationTime(audium::clocks));
        juce::Rectangle<double> rect_tmp(start, getLocalBounds().getY(), width, getLocalBounds().getHeight());
        
        regionView->setBounds(rect_tmp.toNearestInt());
        if (playListItem->isSelected())
            regionView->toFront(false);
    }
}

bool AudioTrackComponent::isInterestedInDragSource (const SourceDetails &dragSourceDetails)
{
    if (dynamic_cast<PlayListItemComponent*>(dragSourceDetails.sourceComponent.get()) != nullptr)
        return true;

    if (dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr)
        return true;
    
    return false;
}

void AudioTrackComponent::itemDragEnter (const SourceDetails &dragSourceDetails)
{
    if (auto playListItemComponent = dynamic_cast<PlayListItemComponent*>(dragSourceDetails.sourceComponent.get()))
    {
        if (playListItemComponent->getPlayListItem()->getRegion()->getAudioTrack() == audioTrack)
        {
            externalDragAndDrop = false; // source details match this track -> no highlight!
        }
        else
        {
            externalDragAndDrop = true;
        }
    }
    else {
        externalDragAndDrop = true;
    }
}

void AudioTrackComponent::itemDragMove (const SourceDetails &dragSourceDetails)
{
    auto x = dragSourceDetails.localPosition.x;
    auto length = 0.01;
    if (auto playListItemComponent = dynamic_cast<PlayListItemComponent*>(dragSourceDetails.sourceComponent.get()))
    {
        // apply x offset
        x -= playListItemComponent->getDraggerControl()->mouseDownOffset.getX();
        if (auto region = playListItemComponent->getPlayListItem()->getRegion())
        {
            length = region->getRegionData(audium::clocks).getLength();
        }
    }
    
    auto start = zoomHandler->xToClocks(x);
    auto end = start + length;
    Range<double> rangeInClocks(start, end);
    
    zoomHandler->getSnapToGridHandler()->publishRange(rangeInClocks);
}

void AudioTrackComponent::itemDragExit (const SourceDetails &dragSourceDetails)
{
    zoomHandler->getSnapToGridHandler()->clearRange();
    externalDragAndDrop = false;
    repaint();
}

void AudioTrackComponent::itemDropped (const SourceDetails &dragSourceDetails)
{
    // undo 
    auto action = std::make_unique<audium::UndoableContainerAction>(audioTrack->getAudioTrackContainer());
    
    auto x = dragSourceDetails.localPosition.x;
    
    // apply x offset
    if (auto playListItemComponent = dynamic_cast<PlayListItemComponent*>(dragSourceDetails.sourceComponent.get()))
        x -= playListItemComponent->getDraggerControl()->mouseDownOffset.getX();
        
    auto pos = zoomHandler->xToClocks(x);
    zoomHandler->snapToGrid(pos);
    
    bool success = false;
    if (auto regionLabel = dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get())) {
        audioTrack->dropSelectedAudioRegions(pos, audium::clocks);
        success = true;
    }
    else if (auto playListItemComponent = dynamic_cast<PlayListItemComponent*>(dragSourceDetails.sourceComponent.get())) {
        audioTrack->dropPlayListItem(playListItemComponent->getPlayListItem(), pos, audium::clocks);
        success = true;
    }
    
    if (success) {
        action->storeNewState();
        auto undoManager = audioTrack->getAudioTrackContainer().getUndoManager();
        undoManager->perform(action.release(), "item dropped");
        undoManager->beginNewTransaction();
    }

    zoomHandler->getSnapToGridHandler()->clearRange();
    externalDragAndDrop = false;
    repaint();
}

