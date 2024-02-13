/*
  ==============================================================================

    PlayListTableListBoxItem.cpp
    Created: 30 Jun 2023 12:58:40pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "PlayListTableListBoxItem.h"
#include "PlayListTableListBoxModel.h"
#include "Interface/Components/RightPanel/PlayListComponent.h"
#include "Interface/Controls/RegionLabel.h"
#include "Engine/AudioRegionContainer.h"
#include "Engine/TransportSourceContainer.h"
#include "Interface/AudiumLookAndFeel.h"
#include "Engine/ActionMessages.h"
#include "Engine/Undo/UndoableContainerAction.h"

void PlayListTableListBoxItem::itemDropped (const SourceDetails &dragSourceDetails)
{
    // Undo: store old state
    auto action = std::make_unique<audium::UndoableContainerAction>(playListModel->getPlayListContainer());
    
    auto before = dragSourceDetails.localPosition.y < getHeight() / 2;
    auto insertIndex = rowNumber + (before ? 0 : 1);
    
    std::cout << "row number: " << rowNumber + (before ? 0 : 1) << std::endl;
    
    if ( PlayListTableListBoxItem* item = dynamic_cast<PlayListTableListBoxItem*>(dragSourceDetails.sourceComponent.get()))
    {
        MoveItemBefore(playListModel->getPlayListContainer()->playListItems,
                       item->rowNumber,
                       insertIndex);
    }
    else if ( RegionLabel* item = dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()))
    {
        playListModel->getPlayListContainer()->createPlayListItem(item->getRowNumber(), insertIndex);
    }
    

    // Undo: store new state
    action->storeNewState();
    // oh dear
    auto undoManager = playListModel->getPlayListContainer()->getAudioRegionContainer().getUndoManager();
    undoManager->perform(action.release(), "Playlist changed");
    undoManager->beginNewTransaction();
    
    
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
    
    auto container = playListModel->getAudioGroup()->getPlayListContainer();
    if (auto r = container->getPlayListItem(rowNumber))
    {
        juce::String text;

        if (columnNumber == 1)
        {
            text = r->getRegion()->getName();
        }
        else if (columnNumber == 2)
        {
            text = "n/a";
        }

        auto textColour = r->getRegion()->getAudioGroup()->getColour();
        g.setColour (selected ? textColour.brighter().brighter() : textColour);
        

        g.setFont (13.0f);
        g.drawText (text, 4, 0, getWidth() - 6, getHeight(), juce::Justification::centredLeft, true);
    }
    

    
    if( insertAfter )
    {
        g.setColour(juce::Colours::red);
        g.fillRect(0, getHeight()-3, getWidth(), 3);
    }
    else if( insertBefore )
    {
        g.setColour(juce::Colours::red);
        g.fillRect(0, 0, getWidth(), 3);
    }
}

void PlayListTableListBoxItem::mouseDoubleClick (const juce::MouseEvent&)
{
    auto group = playListModel->getAudioGroup();
    playListModel->getPlayListScheduler()->setCurrentPositionAtPlayListItemIndex(group, rowNumber);
}

void PlayListTableListBoxItem::timerCallback()
{
    auto group = playListModel->getAudioGroup();
    auto itemPlaying = playListModel->getPlayListScheduler()->getPlayListItemIndexAtCurrentPosition(group);
    
    auto theProgress = 0.0;
    if (itemPlaying == rowNumber)
    {
        theProgress = playListModel->getPlayListScheduler()->getPlayListItemProgress(group, rowNumber);
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
