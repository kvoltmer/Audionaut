/*
  ==============================================================================

    DraggerControl.cpp
    Created: 28 Nov 2024 4:11:04pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "DraggerControl.h"

void DraggerControl::mouseDown (const juce::MouseEvent& e)
{
    if (regionSelector != nullptr)
        regionSelector->setEnabled(false);

    currentDragMode = getDragMode(e.getPosition().getX());
    
    originalBounds = componentToDrag->getBounds();
    
    setSelected(e.mods.isCommandDown() ? !isSelected() : true, false);
    
    auto rangeInClocks = zoomHandler->xToClocks(componentToDrag->getBounds().toDouble().getHorizontalRange());
    zoomHandler->getSnapToGridHandler()->publishRange(rangeInClocks);
    
    mouseDownOffset = getLocalPoint (this, e.position);
    
    repaint();
}

void DraggerControl::mouseUp (const juce::MouseEvent& e)
{
    if (regionSelector != nullptr)
        regionSelector->setEnabled(true);
    
    if (std::abs(e.getOffsetFromDragStart().getX()) > 0)
    {
        auto rangeInClocks = zoomHandler->xToClocks(componentToDrag->getBounds().toDouble().getHorizontalRange());
        zoomHandler->snapToGrid(rangeInClocks);
        commitRangeToEngine(rangeInClocks);
        validateData();
        sendChangeMessage();
    }
    
    // deselect others
    setSelected(isSelected(), !e.mods.isAnyModifierKeyDown());
    
    zoomHandler->getSnapToGridHandler()->clearRange();
}

void DraggerControl::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isAltDown())
    {
        if (juce::DragAndDropContainer* container = juce::DragAndDropContainer::findParentDragContainerFor(componentToDrag))
        {
            container->startDragging("DraggerControl", componentToDrag);
        }
    }
    else
    {
        auto distance = e.getOffsetFromDragStart();
        distance.setY(0); // drag vertically only
        if (std::abs(distance.getX()) > 0)
        {
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
            
            auto rangeInClocks = zoomHandler->xToClocks(componentToDrag->getBounds().toDouble().getHorizontalRange());
            zoomHandler->getSnapToGridHandler()->publishRange(rangeInClocks);
            commitRangeToEngine(rangeInClocks);
        }
    }
}

void DraggerControl::mouseMove (const juce::MouseEvent& e)
{
    updateMouseZone (e);
}
