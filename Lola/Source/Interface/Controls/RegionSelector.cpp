/*
  ==============================================================================

    RegionSelector.cpp
    Created: 25 May 2023 12:37:00pm
    Author:  Klaus Voltmer

  ==============================================================================
*/
#include <iostream>

#include "RegionSelector.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Handlers/SnapToGridHandler.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/PlayList/PlayListScheduler.h"

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
    if (isEnabled())
    {
        //std::cout << "RegionSelector::mouseDown" << std::endl;
        
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

        if (owner->getWidth() - parentPosition.getX() < 10)
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
            audiumEngine->getAudioTrackContainer()->getAudioRegionAdapter().setSelectedRange(juce::Range<double>(), audium::seconds);
        }
        else /// click inside -> modify current selection
        {
            moveStartPos = e.getEventRelativeTo(owner.get()).getMouseDownPosition();
            currentDragMode = getDragMode(e.getPosition().getX());
        }
    }
}

void RegionSelector::mouseDrag (const juce::MouseEvent& e)
{
    if (isEnabled())
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
        
        auto start = zoomHandler->xToClocksWithOffset(dragStartPos.getX());
        auto end = zoomHandler->xToClocksWithOffset(dragEndPos.getX());
        
        // calc engine values
        Range<double> rangeInClocks(start, end);
        if (end < start)
        {
            rangeInClocks = Range<double>(end, start);
        }
        zoomHandler->getSnapToGridHandler()->publishRange(rangeInClocks);
        
        // set value in the engine
        audiumEngine->getAudioTrackContainer()->getAudioRegionAdapter().setSelectedRange(rangeInClocks, audium::clocks);
    }
}

void RegionSelector::createRectangleAndSetBonds()
{
    // create a rectange
    auto rect = Rectangle<int> (dragStartPos, dragEndPos);
    auto headerHeight = owner->getHeaderComponent() ? owner->getHeaderComponent()->getHeight() : 0;
    rect.setTop(owner->getBounds().getY() + headerHeight);
    rect.setHeight(owner->getAllRowsHeight());
    
    // set the size of this component, the width is expanded to simplify selection dragging
    setBounds(rect.expanded(expandedWidth, 0));
}

void RegionSelector::mouseUp (const juce::MouseEvent& e)
{
    if (not audiumEngine->getAudioTrackContainer()->getAudioRegionAdapter().getSelectedRange(audium::clocks).isEmpty())
    {
        grabKeyboardFocus();
    }
    
    auto rangeInClocks = audiumEngine->getAudioTrackContainer()->getAudioRegionAdapter().getSelectedRange(audium::clocks);
    if (!rangeInClocks.isEmpty())
    {
        if (zoomHandler->snapToGrid(rangeInClocks))
        {
            audiumEngine->getAudioTrackContainer()->getAudioRegionAdapter().setSelectedRange(rangeInClocks, audium::clocks);
            updateFromEngine();
        }
    }
    
    zoomHandler->getSnapToGridHandler()->clearRange();
}

void RegionSelector::mouseMove (const juce::MouseEvent& e)
{
    if (isEnabled())
    {
        updateMouseZone (e);
    }
}

void RegionSelector::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& details)
{
    owner->mouseWheelMove(e, details);
}

bool RegionSelector::keyPressed (const KeyPress& key, Component* originatingComponent)
{
    const auto pos = audiumEngine->getAudioTrackContainer()->getAudioRegionAdapter().getSelectedRange(audium::clocks);
    if (!pos.isEmpty())
    {
        if (key.isKeyCode (KeyPress::leftKey))
        {
            zoomHandler->centerView(pos.getStart(), 0.5);
            return true;
        }
        else if (key.isKeyCode (KeyPress::rightKey))
        {
            zoomHandler->centerView(pos.getEnd(), 0.5);
            return true;
        }
        if (key.isKeyCode (KeyPress::upKey))
        {
            // visible center position in clocks
            auto clocks = zoomHandler->xToClocksWithOffset(zoomHandler->getVisibleRange().getLength() * 0.5);
            zoomHandler->zoomIn();
            owner->setMinimumContentWidth(zoomHandler->getContentWidth());
            updateFromEngine();
            zoomHandler->centerView(clocks, 0.5);
            return true;
        }
        else if (key.isKeyCode (KeyPress::downKey))
        {
            // visible center position in clocks
            auto clocks = zoomHandler->xToClocksWithOffset(zoomHandler->getVisibleRange().getLength() * 0.5);
            zoomHandler->zoomOut();
            owner->setMinimumContentWidth(zoomHandler->getContentWidth());
            updateFromEngine();
            zoomHandler->centerView(clocks, 0.5);
            return true;
        }
        else if (key.isKeyCode (KeyPress::escapeKey))
        {
            // clear selection
            audiumEngine->getAudioTrackContainer()->getAudioRegionAdapter().setSelectedRange(juce::Range<double>(), audium::seconds);
            updateFromEngine();
            return true;
        }
    }
    
    return false;
}

void RegionSelector::updateFromEngine()
{
    auto pos = audiumEngine->getAudioTrackContainer()->getAudioRegionAdapter().getSelectedRange(audium::seconds);
    if (pos.isEmpty())
    {
        setSize(0, 0);
    }
    else
    {
        auto start = zoomHandler->secondsToXWithOffset(pos.getStart());
        auto end = zoomHandler->secondsToXWithOffset(pos.getEnd());
        dragStartPos.setX(start);
        dragEndPos.setX(end);
        createRectangleAndSetBonds();
    }
}

void RegionSelector::updateMouseZone (const juce::MouseEvent& e)
{
    //std::cout << "RegionSelector::updateMouseZone" << std::endl;
    
    switch (getDragMode(e.getPosition().getX())) {
        case RegionSelector::leftEdge:
            setMouseCursor (MouseCursor::LeftEdgeResizeCursor);
            break;
        case RegionSelector::rightEdge:
            setMouseCursor (MouseCursor::RightEdgeResizeCursor);
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
