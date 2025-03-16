
#include "Interface/Controls/LoopDraggerControl.h"

void LoopDraggerControl::mouseDown (const juce::MouseEvent& e)
{
    currentDragMode = getDragMode(e.getPosition().getX());
    
    originalBounds = componentToDrag->getBounds();
    
    loopRangeInClocks = zoomHandler->xToClocks(componentToDrag->getBounds().toDouble().getHorizontalRange());
    zoomHandler->getSnapToGridHandler()->publishRange(loopRangeInClocks);
    
    mouseDownOffset = getLocalPoint (this, e.position);
    
    autoScrollOffset = juce::Point<int>(0, 0);
    paintMe = true;
    repaint();
}

void LoopDraggerControl::mouseUp (const juce::MouseEvent& e)
{
    if (std::abs(e.getOffsetFromDragStart().getX()) > 0) {
        auto rangeInClocks = zoomHandler->xToClocks(componentToDrag->getBounds().toDouble().getHorizontalRange());
        zoomHandler->snapToGrid(rangeInClocks);
        
        loopRangeInClocks = rangeInClocks;
                
        NullCheckedInvocation::invoke (onDragEnd);
    }

    zoomHandler->getSnapToGridHandler()->clearRange();
    paintMe = false;
    repaint();
}

void LoopDraggerControl::mouseDrag (const juce::MouseEvent& e)
{

    beginDragAutoRepeat(40);
    autoScrollOffset += zoomHandler->autoScroll(e);

    auto distance = e.getOffsetFromDragStart() + autoScrollOffset;
    
    distance.setY(0); // drag vertically only
    if (std::abs(distance.getX()) > 0)
    {
        NullCheckedInvocation::invoke (onDragStart);
        
        auto bounds = originalBounds;
        
        switch (currentDragMode) {
            case leftEdge:
                bounds.setLeft(juce::jmin(originalBounds.getRight() - minimumWidth, originalBounds.getX() + distance.getX()));
                break;
            case rightEdge:
                bounds.setRight(juce::jmax(originalBounds.getX() + minimumWidth, originalBounds.getRight() + distance.getX()));
                break;
            case middleEdge:
                bounds += distance;
                break;
            default:
                jassertfalse;
                break;
        }
        
        componentToDrag->setBounds (bounds);
        
        loopRangeInClocks = zoomHandler->xToClocks(componentToDrag->getBounds().toDouble().getHorizontalRange());
        zoomHandler->getSnapToGridHandler()->publishRange(loopRangeInClocks);
        NullCheckedInvocation::invoke (onValueChange);
    }
}

void LoopDraggerControl::mouseMove (const juce::MouseEvent& e)
{
    updateMouseZone (e);
}

void LoopDraggerControl::mouseEnter (const MouseEvent& e)
{
    if (regionSelector != nullptr)
        regionSelector->setEnabled(false);
}

void LoopDraggerControl::mouseExit (const MouseEvent& e)
{
    if (regionSelector != nullptr)
        regionSelector->setEnabled(true);
}
