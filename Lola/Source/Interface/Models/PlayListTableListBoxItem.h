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

#pragma once

#include <JuceHeader.h>

class PlayListTableListBoxModel;

class PlayListTableListBoxItem : public juce::Component, public juce::DragAndDropTarget, private juce::Timer
{
public:
    
    PlayListTableListBoxItem(PlayListTableListBoxModel* owner, int columnNumber, int rowNumber) :
        rowNumber(rowNumber),
        playListModel(owner),
        columnNumber(columnNumber)
    {
        startTimer(25);
    }
    
    ~PlayListTableListBoxItem()
    {
        stopTimer();
    }
    
    void paint(juce::Graphics& g) override;
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if( juce::DragAndDropContainer* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
        {
            container->startDragging("PlayListTableListBoxItem", this);
        }
    }
    
    void mouseDown (const juce::MouseEvent& e) override;
    
    void mouseDoubleClick (const juce::MouseEvent&) override;
    
    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) override;
    
    void updateInsertLines(const juce::DragAndDropTarget::SourceDetails &dragSourceDetails);
    
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
    
    void timerCallback() override;
    
    void update(int columnId, int rowNumber, bool isSelected);
    
    void drawLinearProgress (juce::Graphics& g, double progress);
    
    int rowNumber = 0;
    
    const PlayListTableListBoxModel* getPlayListModel() const { return playListModel; }
    
private:
    
    bool insertAfter = false;
    bool insertBefore = false;
    PlayListTableListBoxModel* playListModel;
    
    int columnNumber = 0;
    
    bool selected = false;
    double progress = 0.0;
};
