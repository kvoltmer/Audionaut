//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#include "PlayListTableListBoxItem.h"
#include "PlayListTableListBoxModel.h"
#include "Interface/Components/RightPanel/PlayListComponent.h"
#include "Interface/Controls/RegionLabel.h"
#include "Interface/Controls/TableRegionLabel.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Engine/ActionMessages.h"
#include "Engine/Undo/UndoableContainerAction.h"

bool PlayListTableListBoxItem::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails)
{
    if (dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr)
        return true;
    
    if (dynamic_cast<TableRegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr)
        return true;
    
    if (auto item = dynamic_cast<PlayListTableListBoxItem*>(dragSourceDetails.sourceComponent.get()))
    {
        if (item->getPlayListModel() == playListModel)
        {
            // return true if source details match this model
            return true;
        }
    }
    return false;
}

void PlayListTableListBoxItem::updateInsertLines(const juce::DragAndDropTarget::SourceDetails &dragSourceDetails)
{
    auto before = dragSourceDetails.localPosition.y < getHeight() / 2;
    auto insertIndex = rowNumber + (before ? 0 : 1);
    
    if (auto item = dynamic_cast<PlayListTableListBoxItem*>(dragSourceDetails.sourceComponent.get()))
    {
        //std::cout << "movePlayListItemBefore this: " << rowNumber << " item: " << item->rowNumber << " insert: " << insertIndex << std::endl;
        
        if (rowNumber == item->rowNumber ||
            item->rowNumber == insertIndex)
        {
            repaint();
            return;
        }
    }
    
    if( dragSourceDetails.localPosition.y < getHeight() / 2 )
    {
        insertBefore = true;
        insertAfter = false;
    }
    else
    {
        insertAfter = true;
        insertBefore = false;
    }
    
    repaint();
}

void PlayListTableListBoxItem::itemDropped (const SourceDetails &dragSourceDetails)
{
    auto playListContainer = playListModel->getPlayListContainer();
    
    
    // Undo: store old state
    auto action = std::make_unique<audium::UndoableContainerAction>(playListModel->getAudioTrack()->getAudioTrackContainer());
    
    auto before = dragSourceDetails.localPosition.y < getHeight() / 2;
    auto insertIndex = rowNumber + (before ? 0 : 1);
    
    //std::cout << "row number: " << rowNumber + (before ? 0 : 1) << std::endl;
    
    bool modified = false;
    
    if (auto item = dynamic_cast<PlayListTableListBoxItem*>(dragSourceDetails.sourceComponent.get()))
    {
        if (rowNumber != item->rowNumber &&
            item->rowNumber != insertIndex)
        {
            playListContainer->movePlayListItemBefore(item->rowNumber, insertIndex);
            modified = true;
        }
    }
    else if (dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr ||
             dynamic_cast<TableRegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr)
    {
        playListModel->getAudioTrack()->dropSelectedAudioRegions(insertIndex);
        modified = true;
    }
    
    if (modified)
    {
        // Undo: store new state
        action->storeNewState();
        // oh dear
        auto undoManager = playListModel->getAudiumEngine()->getUndoManager();
        undoManager->perform(action.release(), "Playlist changed");
        undoManager->beginNewTransaction();
    }
    
    hideInsertLines();
}

void PlayListTableListBoxItem::drawLinearProgress (juce::Graphics& g, double progress)
{
    auto background = playListModel->listBox->findColour(audium::listBoxBackgroundColourId);
    background = selected ? background.brighter().brighter() : background;
    if (columnNumber == 1)
    {
        auto foreground = selected ? background.darker() : background.brighter();
        
        auto barBounds = getLocalBounds().toFloat();

        if (progress >= 0.0f && progress <= 1.0f)
        {
            barBounds.setWidth (barBounds.getWidth() * (float) progress);
            g.setColour (foreground);
            g.fillRoundedRectangle (barBounds, (float) 1.f);
        }
    }
}

void PlayListTableListBoxItem::paint(juce::Graphics& g)
{
    if (columnNumber == 1)
    {
        drawLinearProgress(g, progress);
    }
    
    auto container = playListModel->getAudioTrack()->getPlayListContainer();
    if (auto r = container->getPlayListItem(rowNumber))
    {
        juce::String text;
        auto groupColour = r->getRegion()->getAudioTrack()->getColour();
        auto groupHighlightColour = groupColour.brighter().brighter();

        if (columnNumber == 1)
        {
            text = r->getRegion()->getName();
        }
        else if (columnNumber == 2)
        {
            text = "n/a";
        }

        g.setColour (selected ? groupHighlightColour : groupColour);
        g.setFont (13.0f);
        g.drawText (text, 4, 0, std::max(0, getWidth() - 6), getHeight(), juce::Justification::centredLeft, true);
        
        g.setColour(groupColour);
        if( insertAfter )
        {
            g.fillRect(0, getHeight()-3, getWidth(), 3);
        }
        else if( insertBefore )
        {
            g.fillRect(0, 0, getWidth(), 3);
        }
    }
}

void PlayListTableListBoxItem::mouseDown (const juce::MouseEvent& e)
{
    auto track = playListModel->getAudioTrack();
    auto container = track->getPlayListContainer();

    if (auto item = container->getPlayListItem(rowNumber)) {
        
        if (!e.mods.isAnyModifierKeyDown() && !item->isSelected()) {
            track->getAudioTrackContainer().getSelectionManager()->deselectAll();
        }
        else {
            auto objects = track->getAudioTrackContainer().getSelectionManager()->getSelectedObjects();
            for (auto object : objects) {
                if (auto item = dynamic_cast<audium::PlayListItem*>(object.get())) {
                    if (item->getRegion()->getAudioTrack() != track)
                        object->setSelected(false);
                }
            }
        }
        
        if (e.mods.isCommandDown() && item->isSelected()) {
            item->setSelected(false);
        }
        else {
            item->setSelected(true);
        }
        
    }
    
    // pass on the event to the model
    getParentComponent()->mouseDown(e);
        
        

    // update
    track->getAudioTrackContainer().sendActionMessage(audium::updateSelection);
}

void PlayListTableListBoxItem::mouseDoubleClick (const juce::MouseEvent&)
{
    auto track = playListModel->getAudioTrack();
    playListModel->getPlayListScheduler()->setCurrentPositionAtPlayListItemIndex(track, rowNumber);
}

void PlayListTableListBoxItem::timerCallback()
{
    auto track = playListModel->getAudioTrack();
    auto itemPlaying = playListModel->getPlayListScheduler()->getPlayListItemIndexAtCurrentPosition(track);
    
    auto theProgress = 0.0;
    if (itemPlaying == rowNumber)
    {
        theProgress = playListModel->getPlayListScheduler()->getPlayListItemProgress(track, rowNumber);
    }
    else
    {
        theProgress = 0.0;
    }
    
    if (progress != theProgress)
    {
        progress = theProgress;
        repaint();
    }
}

void PlayListTableListBoxItem::update(int columnId, int rowNumber, bool isSelected)
{
    this->columnNumber = columnId;
    this->rowNumber = rowNumber;
    selected = isSelected;
    
    
    repaint();
}
