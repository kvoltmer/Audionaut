//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ChannelGroupHeaderComponent.h"

void ChannelGroupHeaderComponent::paint (juce::Graphics& g)
{
    if (audioTrack->isSelected()) {
        auto colour = findColour(audium::secondaryBackgroundColourId).brighter().withAlpha(0.3f);
        g.fillAll (colour);
    }
    
    g.setColour (juce::Colours::black);
    g.drawRect (getLocalBounds(), 1);   // draw an outline around the component
    
    if (insertBefore) {
        g.setColour(audioTrack->getViewState().getColour());
        g.fillRect(0, 0, getWidth(), 3);
    }
}

bool ChannelGroupHeaderComponent::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails)
{
    if (auto item = dynamic_cast<ChannelGroupHeaderComponent*>(dragSourceDetails.sourceComponent.get()))
        return true;
    
    return false;
}

void ChannelGroupHeaderComponent::updateInsertLines(const juce::DragAndDropTarget::SourceDetails &dragSourceDetails)
{
    if (auto trackHeader = dynamic_cast<ChannelGroupHeaderComponent*>(dragSourceDetails.sourceComponent.get())) {

        if (audioTrack->getId() == trackHeader->getAudioTrack()->getId()) {
            hideInsertLines();
            return;
        }
        
        insertBefore = true;
    }

    repaint();
}

void ChannelGroupHeaderComponent::itemDropped (const SourceDetails &dragSourceDetails)
{
    if (auto channelComponent = dynamic_cast<ChannelGroupHeaderComponent*>(dragSourceDetails.sourceComponent.get())) {
        
        if (audioTrack->getId() == channelComponent->getAudioTrack()->getId()) {
            
            hideInsertLines();
            return;
        }
        

        // Undo: store old state
        auto action = std::make_unique<audium::UndoableContainerAction>(getAudioTrack()->getAudioTrackContainer());

        std::cout << "ChannelGroupHeaderComponent::MoveItemBefore -> currentIndex: " << channelComponent->getAudioTrack()->getId() << " indexOfItemToPlaceBefore " << audioTrack->getId() << std::endl;


        audium::MoveItemBefore(getAudioTrack()->getAudioTrackContainer().audioTracks,
                               channelComponent->getAudioTrack()->getId(),
                               audioTrack->getId());

        // Undo: store new state
        action->storeNewState();
        auto undoManager = audioTrack->getAudioTrackContainer().getUndoManager();
        undoManager->perform(action.release(), "Channels changed");
        undoManager->beginNewTransaction();
        
    }
    hideInsertLines();
}
