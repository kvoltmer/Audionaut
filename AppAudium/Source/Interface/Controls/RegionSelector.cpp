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
#include "Engine/AudiumEngine.h"
#include "Engine/TransportSourceContainer.h"

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
    /// TODO: get rid of this workaround: maybe attach mouse listener to the viewport
    if (owner->getHeight() - parentPosition.getY() < 10 ||
        parentPosition.getY() < owner->getHeaderComponent()->getHeight())
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
        audiumEngine->getAudioRegionContainer()->clearSelection();
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
    
    auto start = zoomHandler->xToTimeWithOffset(dragStartPos.getX());
    auto end = zoomHandler->xToTimeWithOffset(dragEndPos.getX());
    
    // calc engine values
    Range<double> pos(start, end);
    if (end < start)
    {
        pos = Range<double>(end, start);
    }
    
    // set value in the engine
    audiumEngine->getAudioRegionContainer()->setRegionPosition(pos);
}

void RegionSelector::createRectangleAndSetBonds()
{
    // create a rectange
    auto rect = Rectangle<int> (dragStartPos, dragEndPos);
    
    rect.setTop(owner->getBounds().getY() + owner->getHeaderComponent()->getHeight());
    rect.setHeight(owner->getAllRowsHeight());
    
    // set the size of this component, the width is expanded to simplify selection dragging
    setBounds(rect.expanded(expandedWidth, 0));
}

void RegionSelector::mouseUp (const juce::MouseEvent& e)
{
    // set transport position if not currently playing
    if (!avoidDragging &&
        !audiumEngine->getTransportSourceContainer()->isPlaying() &&
        getBounds().getWidth() > 1)
    {
        auto pos = 0.0;
        if (dragEndPos.getX() < dragStartPos.getX())
        {
            pos = zoomHandler->xToTimeWithOffset(dragEndPos.getX());
        }
        else
        {
            pos = zoomHandler->xToTimeWithOffset(dragStartPos.getX());
        }
        audiumEngine->getTransportSourceContainer()->setPosition (pos);
    }
    
}

void RegionSelector::mouseMove (const juce::MouseEvent& e)
{
    updateMouseZone (e);
}

void RegionSelector::updateFromEngine()
{
    auto pos = audiumEngine->getAudioRegionContainer()->getRegionPosition();
    if (pos.isEmpty())
    {
        setSize(0, 0);
    }
    else
    {
        auto start = zoomHandler->timeToXWithOffset(pos.getStart());
        auto end = zoomHandler->timeToXWithOffset(pos.getEnd());
        dragStartPos.setX(start);
        dragEndPos.setX(end);
        createRectangleAndSetBonds();
    }
}

void RegionSelector::updateMouseZone (const juce::MouseEvent& e)
{
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
