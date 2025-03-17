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

#include "ChannelsComponent.h"

bool ChannelsComponent::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails)
{
    if (dynamic_cast<ChannelComponent*>(dragSourceDetails.sourceComponent.get()) != nullptr ||
        dynamic_cast<ChannelGroupHeaderComponent*>(dragSourceDetails.sourceComponent.get()) != nullptr) {
        return true;
    }
    return false;
}

int ChannelsComponent::getListBoxHeight() const
{
    auto height = audioChannelsListBox->getHeaderComponent()->getHeight();
    for (auto r = 0; r < audioChannelsListBoxModel->getNumRows(); r++) {
        height += audioChannelsListBoxModel->getRowHeight(r);
    }
    
    auto verticalOffset = static_cast<int>(getVerticalScrollOffset());

    return height - verticalOffset;
}

void ChannelsComponent::itemDragEnter (const SourceDetails &dragSourceDetails)
{
    if (dragSourceDetails.localPosition.y > getListBoxHeight()) {
        itemDrag = true;
        repaint();
    }
}

void ChannelsComponent::itemDragMove (const SourceDetails &dragSourceDetails)
{
    if (dragSourceDetails.localPosition.y > getListBoxHeight()) {
        itemDrag = true;
        repaint();
    }
}

void ChannelsComponent::itemDragExit (const SourceDetails &dragSourceDetails)
{
    itemDrag = false;
    repaint();
}


void ChannelsComponent::itemDropped (const SourceDetails &dragSourceDetails)
{
    if (dragSourceDetails.localPosition.y > getListBoxHeight()) {
        
        auto trackContainer = audiumEngine->getAudioTrackContainer();
        
        if (dynamic_cast<ChannelComponent*>(dragSourceDetails.sourceComponent.get())) {
            trackContainer->copySelectedChannelsToNewTrack();
        }
        else if (auto channelGroupHeaderComponent = dynamic_cast<ChannelGroupHeaderComponent*>(dragSourceDetails.sourceComponent.get())) {
            
            // undo
            auto action = std::make_unique<audium::UndoableContainerAction>(*trackContainer.get());
            auto currentIndex = channelGroupHeaderComponent->getAudioTrack()->getId();
            auto insertIndex = audiumEngine->getAudioTrackContainer()->audioTracks.size();
            std::cout << "ChannelGroupHeaderComponent::MoveItemBefore -> currentIndex: " << currentIndex << " indexOfItemToPlaceBefore " << insertIndex << std::endl;

            audium::MoveItemBefore(audiumEngine->getAudioTrackContainer()->audioTracks,
                                   currentIndex,
                                   insertIndex);
            // undo
            action->storeNewState();
            auto undoManager = audiumEngine->getUndoManager();
            undoManager->perform(action.release(), "track dragged");
            undoManager->beginNewTransaction();

        }
    }

    itemDrag = false;
    repaint();
}


void ChannelsComponent::paint (juce::Graphics& g)
{
    if (itemDrag) {
        
        auto height = getListBoxHeight();
        
        g.setColour(findColour(audium::secondaryBackgroundColourId).brighter().withAlpha(0.5f));
        g.fillRect(0, height, getWidth(), getHeight() - height);
    }
}
