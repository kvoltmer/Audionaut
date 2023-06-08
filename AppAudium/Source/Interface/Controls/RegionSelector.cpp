/*
  ==============================================================================

    RegionSelector.cpp
    Created: 25 May 2023 12:37:00pm
    Author:  Klaus Voltmer

  ==============================================================================
*/
#include <iostream>

#include "RegionSelector.h"
#include "Interface/Components/WaveFormComponent.h"
#include "Engine/AudioRegionContainer.h"

void RegionSelector::paint (Graphics& g)
{
    auto thumbArea = getLocalBounds().reduced(expandedWidth, 0);
    g.setColour (Colours::white);
    g.drawRoundedRectangle (thumbArea.toFloat(), 3.0f, 1.5f);
    g.setColour (Colour(Colours::white).withAlpha(0.125f));
    g.fillRoundedRectangle (thumbArea.toFloat(), 3.0f);
}

void RegionSelector::mouseDown (const juce::MouseEvent& e)
{
    
    /// click outside -> reset current selection
    auto parentPosition = e.getEventRelativeTo(owner.get()).getPosition();
    
    /// filter mouse postion to avoid scrollbar conflict
    /// TODO: maybe attach mouse listener to the viewport
    if (owner->getHeight() - parentPosition.getY() < 10)
    {
        avoidDragging = true;
        return;
    }
    
    avoidDragging = false;
    
    /// click outside the selection?
    if (!getBoundsInParent().contains(parentPosition))
    {
        setSize (0, 0);
        dragStartPos = e.getEventRelativeTo(owner.get()).getMouseDownPosition();
        currentDragMode = RegionSelector::outsideEdge;
        audioRegionContainer->clearSelection();
    }
    else /// click inside -> modify current selection
    {
        moveStartPos = e.getEventRelativeTo(owner.get()).getMouseDownPosition();
        currentDragMode = getDragMode(e.getPosition().getX());
    }
}

void RegionSelector::mouseDrag (const juce::MouseEvent& e)
{
    // filter mouse postion to avoid scrollbar conflict
    /// TODO: maybe attach mouse listener to the viewport
    auto parentPosition = e.getEventRelativeTo(owner.get()).getPosition();
    if (avoidDragging ||
        owner->getHeight() - parentPosition.getY() < 10)
    {
        return;
    }
    
    auto delta =  e.getEventRelativeTo(owner.get()).getPosition().getX() - moveStartPos.getX();
    moveStartPos = e.getEventRelativeTo(owner.get()).getPosition();
    
    switch (currentDragMode) {
        case RegionSelector::leftEdge:
            if (dragStartPos.getX() < dragEndPos.getX())
            {
                dragStartPos.setX(dragStartPos.getX() + delta);
            }
            else
            {
                dragEndPos.setX(dragEndPos.getX() + delta);
            }
            break;
        case RegionSelector::rightEdge:
            if (dragStartPos.getX() < dragEndPos.getX())
            {
                dragEndPos.setX(dragEndPos.getX() + delta);
            }
            else
            {
                dragStartPos.setX(dragStartPos.getX() + delta);
            }
            break;
        case RegionSelector::middleEdge:
            dragStartPos.setX(dragStartPos.getX() + delta);
            dragEndPos.setX(dragEndPos.getX() + delta);
            break;
        case RegionSelector::outsideEdge:
            dragEndPos = e.getEventRelativeTo(owner.get()).getPosition();
            break;
    
        default:
            break;
    }

    createRectangleAndSetBonds();
    
    
    auto offset = zoomHandler->getVisibleRange().getStart();
    auto start = jmax (0.0, zoomHandler->xToTime ((double) dragStartPos.getX() + offset));
    auto end = jmax (0.0, zoomHandler->xToTime ((double) dragEndPos.getX() + offset));
    
    // calc engine values
    Range<double> pos(start, end);
    if (end < start)
    {
        pos = Range<double>(end, start);
    }
    
    // set value in the engine
    audioRegionContainer->setRegionPosition(pos);
}

void RegionSelector::createRectangleAndSetBonds()
{
    // create a rectange
    auto rect = Rectangle<int> (dragStartPos, dragEndPos);
    
    rect.setTop(owner->getBounds().getY());
    rect.setHeight(owner->getAllRowsHeight());
    
    // set the size of this component, the width is expanded to simplify selection dragging
    setBounds(rect.expanded(expandedWidth, 0));
}

void RegionSelector::mouseUp (const juce::MouseEvent&)
{
}

void RegionSelector::mouseMove (const juce::MouseEvent& e)
{
    updateMouseZone (e);
    
    //auto pos = e.getEventRelativeTo (owner.get()).position.toInt();
    //auto row = owner->getRowContainingPosition (pos.x, pos.y);
    //std::cout << "x " << pos.getX() << std::endl;
    //std::cout << row << " --------" << pos.getX() << "---------" << pos.getY() << std::endl;
    
}

void RegionSelector::updateFromEngine()
{
    auto pos = audioRegionContainer->getRegionPosition();
    if (pos.isEmpty())
    {
        setSize(0, 0);
    }
    else
    {
        auto start = zoomHandler->timeToX(pos.getStart());
        auto end = zoomHandler->timeToX(pos.getEnd());
        auto offset = zoomHandler->getVisibleRange().getStart();
        
        dragStartPos.setX(start - offset);
        dragEndPos.setX(end - offset);
        createRectangleAndSetBonds();
    }
}

void RegionSelector::updateMouseZone (const juce::MouseEvent& e)
{
    //std::cout << "updateMouseZone " << e.getPosition().getX() << " " << getWidth() <<  std::endl;
    
    auto x = e.getPosition().getX();
    
    switch (getDragMode(x)) {
        case RegionSelector::leftEdge:
        case RegionSelector::rightEdge:
            setMouseCursor (MouseCursor::LeftRightResizeCursor);
            break;
        case RegionSelector::middleEdge:
            setMouseCursor (MouseCursor::DraggingHandCursor);
            break;
        default:
            break;
    }
}

const RegionSelector::Edge RegionSelector::getDragMode(int x) const
{
    //std::cout << "x " << x << std::endl;
    if (x < borderSize)
    {
        return RegionSelector::leftEdge;
    }
    else if (getWidth() - x < borderSize)
    {
        return RegionSelector::rightEdge;
    }
    else
    {
        return RegionSelector::middleEdge;
    }
}
