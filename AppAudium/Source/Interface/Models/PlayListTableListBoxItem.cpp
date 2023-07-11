/*
  ==============================================================================

    PlayListTableListBoxItem.cpp
    Created: 30 Jun 2023 12:58:40pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "PlayListTableListBoxItem.h"
#include "PlayListTableListBoxModel.h"
#include "Interface/Components/PlayListComponent.h"
#include "Interface/Controls/RegionEditor.h"
#include "Engine/AudioRegionContainer.h"
#include "Engine/TransportSourceProvider.h"
#include "Interface/AudiumLookAndFeel.h"

void PlayListTableListBoxItem::itemDropped (const SourceDetails &dragSourceDetails)
{
    auto before = dragSourceDetails.localPosition.y < getHeight() / 2;
    auto insertIndex = rowNumber + (before ? 0 : 1);
    
    std::cout << "row number: " << rowNumber + (before ? 0 : 1) << std::endl;
    
    if ( PlayListTableListBoxItem* item = dynamic_cast<PlayListTableListBoxItem*>(dragSourceDetails.sourceComponent.get()))
    {
        MoveItemBefore(playListModel->getPlayListContainer()->playListItems,
                       item->rowNumber,
                       insertIndex);
    }
    else if ( RegionEditor* item = dynamic_cast<RegionEditor*>(dragSourceDetails.sourceComponent.get()))
    {
        playListModel->getPlayListContainer()->createPlayListItem(item->getRowNumber(), insertIndex);
    }
    
    // TODO: use your own update system
    playListModel->listBox->owner->triggerAsyncUpdate();
    
    hideInsertLines();
}

void PlayListTableListBoxItem::drawLinearProgress (juce::Graphics& g, double progress)
{
    auto background = playListModel->listBox->findColour(selected ? audium::defaultHighlightColourId :
                                                               audium::secondaryBackgroundColourId);
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
    auto background = playListModel->listBox->findColour(selected ? audium::defaultHighlightColourId :
                                                               audium::secondaryBackgroundColourId);
    g.fillAll (background);
    
    if (columnNumber == 1)
    {
        if (playListModel->getPlayListScheduler()->isPlaying())
        {
            auto itemPlaying = playListModel->getPlayListScheduler()->getPlayListItemIndex();
            if (itemPlaying == rowNumber)
            {
                if (!isTimerRunning())
                {
                    startTimerHz(25);
                }
                
                auto progress = playListModel->getPlayListScheduler()->getPlayListItemProgress(rowNumber);
                drawLinearProgress(g, progress);
            }
            else if (isTimerRunning())
            {
                stopTimer();
            }
        }
        else if (isTimerRunning())
        {
            stopTimer();
        }
    }
    
    
    if (auto r = playListModel->getPlayListContainer()->getPlayListItem(rowNumber))
    {
        juce::String text;

        if (columnNumber == 1)
        {
            text = r->getRegion()->name;
        }
        else if (columnNumber == 2)
        {
            text = "n/a";
        }

        if (selected)
            g.setColour (playListModel->listBox->findColour (audium::defaultHighlightedTextColourId));
        else
            g.setColour (playListModel->listBox->findColour (audium::defaultTextColourId));

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
    playListModel->getPlayListScheduler()->setPlayListItemIndex(rowNumber);
    //std::cout << "mouseDoubleClick" << rowNumber << std::endl;
}

void PlayListTableListBoxItem::timerCallback()
{
    repaint();
}
