

#include "ChannelsComponent.h"

bool ChannelsComponent::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails)
{
    if (auto item = dynamic_cast<ChannelComponent*>(dragSourceDetails.sourceComponent.get()))
        return true;

    return false;
}

int ChannelsComponent::getListBoxHeight() const
{
    auto height = audioChannelsListBox->getHeaderComponent()->getHeight();
    for (auto r = 0; r < audioChannelsListBoxModel->getNumRows(); r++) {
        height += audioChannelsListBoxModel->getRowHeight(r);
    }
    return height;
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
    if (auto channelComponent = dynamic_cast<ChannelComponent*>(dragSourceDetails.sourceComponent.get())) {
        
        if (dragSourceDetails.localPosition.y > getListBoxHeight()) {
            
            auto trackContainer = audiumEngine->getAudioTrackContainer();
            auto selectedObjects = trackContainer->getSelectionManager()->getSelectedObjects();
            if (selectedObjects.size() > 0) {
            
                // undo
                auto action = std::make_unique<audium::UndoableContainerAction>(*trackContainer.get());
                
                // create new audio track
                auto audioTrack = trackContainer->createNewAudioTrack(juce::String());
                audioTrack->setColour(trackContainer->getNewAudioTrackColour());
                
                // copy selected channels
                for (auto object : selectedObjects) {
                    
                    if (auto audioChannel = std::dynamic_pointer_cast<AudioChannel>(object)) {
                        json j;
                        audioChannel->getAudioTrack().writeChannelToJson(j, audioChannel.get());
                        audioTrack->mergeChannelFromJson(j);
                    }
                }
                
                // undo
                action->storeNewState();
                auto undoManager = audiumEngine->getUndoManager();
                undoManager->perform(action.release(), "Channel dragged");
                undoManager->beginNewTransaction();
            }
        }
    

    }
    itemDrag = false;
    repaint();
}


void ChannelsComponent::paint (juce::Graphics& g)
{
    if (itemDrag) {
        auto height = audioChannelsListBox->getHeaderComponent()->getHeight();
        for (auto r = 0; r < audioChannelsListBoxModel->getNumRows(); r++) {
            height += audioChannelsListBoxModel->getRowHeight(r);
        }
        g.setColour(findColour(audium::secondaryBackgroundColourId).brighter().withAlpha(0.5f));
        g.fillRect(0, height, getWidth(), getHeight() - height);
    }
}
