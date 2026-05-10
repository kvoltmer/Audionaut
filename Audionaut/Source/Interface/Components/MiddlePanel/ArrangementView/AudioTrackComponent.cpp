//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

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

class AudioTrackComponent::ItemComponent  : public Component
{
public:
    ItemComponent (AudioTrackComponent& owner_) : owner (owner_) {}

    void update (const int newItem)
    {
        const auto rowHasChanged = (item != newItem);

        if (rowHasChanged)
        {
            repaint();

            if (rowHasChanged)
                item = newItem;
        }

        if (auto* m = owner.getModel())
        {
            customComponent.reset (m->refreshComponentForItem (newItem, customComponent.release()));

            if (customComponent != nullptr)
            {
                addAndMakeVisible (customComponent.get());
                customComponent->setBounds (getLocalBounds());

                setFocusContainerType (FocusContainerType::focusContainer);
            }
            else
            {
                setFocusContainerType (FocusContainerType::none);
            }
        }
    }

    AudioTrackComponent& owner;
    std::unique_ptr<Component> customComponent;
    int item = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ItemComponent)
};

void AudioTrackComponent::refreshComponent (std::shared_ptr<audium::AudioTrack> track, bool forceRebuildComponents)
{
    if (audioTrack != track) {
        audioTrack = track;
        model->setAudioTrack(track);
        forceRebuildComponents = true;
    }
    
    updateContents(forceRebuildComponents);
}


void AudioTrackComponent::updateContents(bool forceRebuildComponents)
{
    if (forceRebuildComponents) {
        itemComponents.clear();
        removeAllChildren();
    }
    
    const size_t numNeeded = (model != nullptr) ? model->getNumRows() : 0;
    itemComponents.resize (jmin (numNeeded, itemComponents.size()));
    while (numNeeded > itemComponents.size())
    {
        itemComponents.emplace_back (new ItemComponent (*this));
        addAndMakeVisible (*itemComponents.back());
    }
        
    for (auto item = 0; item < itemComponents.size(); ++item) {
        
        // TODO: optimise -> if (auto* rowComp = getComponentForRowIfOnscreen (row))
        
        if (auto itemComp = dynamic_cast<ItemComponent*>(itemComponents[item].get())) {
            
            auto range = model->getRangeForItem(item);
            juce::Rectangle<double> rect_tmp(range.getStart(),
                                             getLocalBounds().getY(),
                                             range.getLength(),
                                             getLocalBounds().getHeight());
            itemComp->setBounds(rect_tmp.getSmallestIntegerContainer());
            itemComp->update (item);
            itemComp->repaint();
        }
    }
}

void AudioTrackComponent::resized()
{
}

bool AudioTrackComponent::isInterestedInDragSource (const SourceDetails &dragSourceDetails)
{
    if (auto playListItemComp = dynamic_cast<PlayListItemComponent*>(dragSourceDetails.sourceComponent.get())) {
        return true;
    }

    if (dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr)
        return true;
    
    if (dynamic_cast<PlayListTableListBoxItem*>(dragSourceDetails.sourceComponent.get()) != nullptr)
        return true;
    
    if (dynamic_cast<juce::FileTreeComponent*>(dragSourceDetails.sourceComponent.get()) != nullptr)
        return true;
    
    return false;
}

void AudioTrackComponent::itemDragEnter (const SourceDetails &dragSourceDetails)
{
    if (auto playListItemComponent = dynamic_cast<PlayListItemComponent*>(dragSourceDetails.sourceComponent.get())) {
        if (playListItemComponent->getPlayListItem()->getRegion()->getAudioTrack() == audioTrack) {
            externalDragAndDrop = false; // source details match this track -> no highlight!
        }
        else {
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
    if (dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr) {
        audioTrack->dropSelectedAudioRegions(pos, audium::clocks);
        success = true;
    }
    else if (auto playListItemComponent = dynamic_cast<PlayListItemComponent*>(dragSourceDetails.sourceComponent.get())) {
        audioTrack->dropPlayListItem(playListItemComponent->getPlayListItem(), pos, audium::clocks);
        success = true;
    }
    else if (auto playListTableListBoxItem = dynamic_cast<PlayListTableListBoxItem*>(dragSourceDetails.sourceComponent.get())) {
        audioTrack->dropPlayListItem(playListTableListBoxItem->getPlayListItem(), pos, audium::clocks, true);
        success = true;
    }
    else if (auto tree = dynamic_cast<juce::FileTreeComponent*>(dragSourceDetails.sourceComponent.get())) {
        std::cout << "TODO" << std::endl;
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

