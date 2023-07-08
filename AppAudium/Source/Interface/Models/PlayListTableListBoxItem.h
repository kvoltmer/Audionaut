/*
  ==============================================================================

    PlayListTableListBoxItem.h
    Created: 30 Jun 2023 11:58:41am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class PlayListTableListBoxModel;

struct PlayListTableListBoxItem : public juce::Component, public juce::DragAndDropTarget
{
    PlayListTableListBoxItem(PlayListTableListBoxModel* owner, int columnNumber, int rowNumber) :
        playListModel(owner),
        columnNumber(columnNumber),
        rowNumber(rowNumber)
    {
    }

//    PlayListTableListBoxItem(const PlayListTableListBoxItem& other)
//    {
//        c = other.c;
//        idNum = other.idNum;
//        owner = other.owner;
//    }
    
    void paint(juce::Graphics& g) override;
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if( juce::DragAndDropContainer* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
        {
            container->startDragging("PlayListTableListBoxItem", this);
        }
    }
    
    void mouseDown (const juce::MouseEvent& e) override
    {
        getParentComponent()->mouseDown(e);
    }
    
    void mouseDoubleClick (const juce::MouseEvent&) override;
    
    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) override
    {
        return true;
    }
    
    void updateInsertLines(const juce::DragAndDropTarget::SourceDetails &dragSourceDetails)
    {
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
    
    void hideInsertLines()
    {
        insertBefore = false;
        insertAfter = false;
        
        repaint();
    }
    void itemDragEnter (const SourceDetails &dragSourceDetails) override
    {
        updateInsertLines(dragSourceDetails);
    }
    
    void itemDragMove (const SourceDetails &dragSourceDetails) override
    {
        updateInsertLines(dragSourceDetails);
    }
    
    void itemDragExit (const SourceDetails &dragSourceDetails) override
    {
        hideInsertLines();
    }
    
    void itemDropped (const SourceDetails &dragSourceDetails) override;
    
    bool shouldDrawDragImageWhenOver () override
    {
        return true;
    }
    
    void update(int columnId, int rowNumber, bool isSelected)
    {
        this->columnNumber = columnId;
        this->rowNumber = rowNumber;
        selected = isSelected;
        repaint();
    }
    
    
    bool insertAfter = false;
    bool insertBefore = false;
    PlayListTableListBoxModel* playListModel;
    
    int columnNumber;
    int rowNumber;
    bool selected = false;
};
