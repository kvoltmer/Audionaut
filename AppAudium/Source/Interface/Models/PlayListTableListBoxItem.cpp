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

void PlayListTableListBoxItem::itemDropped (const SourceDetails &dragSourceDetails)
{
    auto before = dragSourceDetails.localPosition.y < getHeight() / 2;
    auto insertIndex = rowNumber + (before ? 0 : 1);
    
    std::cout << "row number: " << rowNumber + (before ? 0 : 1) << std::endl;
    
    if ( PlayListTableListBoxItem* item = dynamic_cast<PlayListTableListBoxItem*>(dragSourceDetails.sourceComponent.get()))
    {
        MoveItemBefore(playListModel->playListContainer->playListItems,
                       item->rowNumber,
                       insertIndex);
    }
    else if ( RegionEditor* item = dynamic_cast<RegionEditor*>(dragSourceDetails.sourceComponent.get()))
    {
        playListModel->playListContainer->createPlayListItem(item->getRowNumber(), insertIndex);
    }
    
    // TODO: use your own update system
    playListModel->listBox->owner->triggerAsyncUpdate();
    
    hideInsertLines();
}

void PlayListTableListBoxItem::paint(juce::Graphics& g)
{
    if (selected)
    {
        g.fillAll (playListModel->listBox->findColour(audium::defaultHighlightColourId));
    }
    
    
    if (auto r = playListModel->playListContainer->getPlayListItem(rowNumber))
    {
        juce::String text;

        if (columnNumber == 1)
        {
            text = r->getRegion()->name;
        }
        else if (columnNumber == 2)
        {
            text = "x";
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
