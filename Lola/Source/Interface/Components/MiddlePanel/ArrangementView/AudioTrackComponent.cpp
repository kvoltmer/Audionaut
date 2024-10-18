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
    }
}

bool AudioTrackComponent::isInterestedInDragSource (const SourceDetails &dragSourceDetails)
{
    if (auto regionLabel = dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()))
    {
        if (regionLabel->getRegion() &&
            regionLabel->getRegion()->getAudioTrack() == audioTrack)
        {
            // return true if source details match this track
            return true;
        }
    }
    return false;
}

void AudioTrackComponent::itemDragMove (const SourceDetails &dragSourceDetails)
{
    auto x = dragSourceDetails.localPosition.x;
    auto start = zoomHandler->xToClocks(x);
    auto end = start + 0.01;
    Range<double> rangeInClocks(start, end);
    
    zoomHandler->getSnapToGridHandler()->publishRange(rangeInClocks);
}

void AudioTrackComponent::itemDragExit (const SourceDetails &dragSourceDetails)
{
    zoomHandler->getSnapToGridHandler()->clearRange();
    repaint();
}

void AudioTrackComponent::itemDropped (const SourceDetails &dragSourceDetails)
{

    if ( RegionLabel* regionLabel = dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()))
    {
        // undo action stores current state
        auto action = std::make_unique<audium::UndoableContainerAction>(audioTrack->getAudioTrackContainer());
                
        auto start = zoomHandler->xToClocks(dragSourceDetails.localPosition.x);
        zoomHandler->snapToGrid(start);
        
        // convert row number to region pointer
        auto region = audiumEngine->getAudioTrackContainer()->getAudioRegionAdapter().getRegion(regionLabel->getRowNumber());
        jassert(region);
        if (region != nullptr)
        {
            juce::Range<double> position(start, start + region->getRegionData(audium::clocks).getLength());
            if (audioTrack->getPlayListContainer()->createPlayListItemAtPositionUI(region, position, audium::clocks) != nullptr)
            {
                action->storeNewState();
                auto undoManager = audioTrack->getAudioTrackContainer().getUndoManager();
                undoManager->perform(action.release(), "Playlist modified");
                undoManager->beginNewTransaction();
            }
        }
    }
    
    zoomHandler->getSnapToGridHandler()->clearRange();
    repaint();
}

void AudioTrackComponent::filesDropped (const StringArray& filenames, int x, int y)
{
    if ( !filenames.isEmpty())
    {
        auto action = std::make_unique<audium::UndoableContainerAction>(*audiumEngine->getAudioTrackContainer());
        
        auto start = zoomHandler->xToClocks(x);
        zoomHandler->snapToGrid(start);
        
        std::function<void (std::string)> callback = [](std::string error) {
            juce::NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                        "Failed to open File.",
                                                        "Failed to open: " + juce::String(error));
        };
        
        if (audioTrack->addAudioFiles(filenames, start, true, callback))
        {
            action->storeNewState();
            audiumEngine->getUndoManager()->perform(action.release(), "File(s) dropped");
            audiumEngine->getUndoManager()->beginNewTransaction();
        }
    }

    // call base class!
    AudioTrackBaseComponent::filesDropped(filenames, x, y);
}
