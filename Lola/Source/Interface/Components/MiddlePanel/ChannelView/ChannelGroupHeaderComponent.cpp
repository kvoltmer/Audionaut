//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
        g.setColour(audioTrack->getColour());
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
