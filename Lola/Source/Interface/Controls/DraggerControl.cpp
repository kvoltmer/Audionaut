/*
  ==============================================================================

    DraggerControl.cpp
    Created: 28 Nov 2024 4:11:04pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Interface/Controls/DraggerControl.h"

void DraggerControl::mouseDown (const juce::MouseEvent& e)
{
    currentDragMode = getDragMode(e.getPosition().getX());
    
    originalBounds = componentToDrag->getBounds();
    
    if (e.mods.isShiftDown()) {
        shiftSelect();
    }
    else {
        bool deselectOthers = !isSelected() && !e.mods.isAnyModifierKeyDown();
        setSelected(e.mods.isCommandDown() ? !isSelected() : true, deselectOthers);
    }
    auto rangeInClocks = zoomHandler->xToClocks(componentToDrag->getBounds().toDouble().getHorizontalRange());
    zoomHandler->getSnapToGridHandler()->publishRange(rangeInClocks);
    
    mouseDownOffset = getLocalPoint (this, e.position);
    
    autoScrollOffset = juce::Point<int>(0, 0);
    
    repaint();
}

void DraggerControl::mouseUp (const juce::MouseEvent& e)
{
    if (std::abs(e.getOffsetFromDragStart().getX()) > 0) {
        auto rangeInClocks = zoomHandler->xToClocks(componentToDrag->getBounds().toDouble().getHorizontalRange());
        zoomHandler->snapToGrid(rangeInClocks);
        commitRangeToEngine(rangeInClocks);
        validateData();
        if (undoableContainerAction != nullptr) {
            undoableContainerAction->storeNewState();
            audiumEngine->getUndoManager()->perform(undoableContainerAction.release(), "Modify Item");
            audiumEngine->getUndoManager()->beginNewTransaction();
        }
        sendChangeMessage();
    }

    zoomHandler->getSnapToGridHandler()->clearRange();
}

void DraggerControl::mouseDrag (const juce::MouseEvent& e)
{
    auto container = juce::DragAndDropContainer::findParentDragContainerFor(componentToDrag);
    auto dragActive = (container != nullptr && container->isDragAndDropActive());
    auto dragVertrically = (std::abs(e.getDistanceFromDragStartY()) > 20 && std::abs(e.getDistanceFromDragStartX()) < 20);
    
    beginDragAutoRepeat(40);
    autoScrollOffset += zoomHandler->autoScroll(e);

    if (e.mods.isAltDown() ||
        dragVertrically) {

        if (container != nullptr)
            container->startDragging("DraggerControl", componentToDrag);
    }
    else if (!dragActive)
    {
        auto distance = e.getOffsetFromDragStart() + autoScrollOffset;
        
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

void DraggerControl::mouseEnter (const MouseEvent& e)
{
    if (regionSelector != nullptr)
        regionSelector->setEnabled(false);
}

void DraggerControl::mouseExit (const MouseEvent& e)
{
    if (regionSelector != nullptr)
        regionSelector->setEnabled(true);
}

bool DraggerControl::keyPressed (const KeyPress& key, Component* originatingComponent)
{
    if (key.isKeyCode (KeyPress::deleteKey) || key.isKeyCode (KeyPress::backspaceKey))
    {
        audiumEngine->getAudioTrackContainer()->deleteSelectedObjects();
        return true;
    }
    else
    {
        // send update to redraw
        audiumEngine->getAudioTrackContainer()->sendActionMessage(updateArrangementAction);
        return false;
    }
}

void DraggerControl::commitData(const juce::Range<double> newData, audium::TimeContextType context)
{
    // undo
    if (undoableContainerAction == nullptr)
    {
        undoableContainerAction = std::make_unique<audium::UndoableContainerAction>(*audiumEngine->getAudioTrackContainer(), false);
    }
 
    commitPositionData(*positionableObject.get(), newData, context);
}

bool DraggerControl::commitPositionData(const PositionableBase &positionableBase,
                                        const juce::Range<double> newRange,
                                        const audium::TimeContextType context)
{
    // apply for all selected items
    auto selectedItems = audiumEngine->getAudioTrackContainer()->getSelectionManager()->getSelectedObjects();
    auto diff = 0.0;
    
    switch (currentDragMode)
    {
        case leftEdge:
            diff = newRange.getStart() - positionableBase.getAbsolutePosition(context);
            if (std::abs(diff) > 0.0) {
                for (auto item : selectedItems) {
                    if (auto pItem = dynamic_cast<PositionableBase*>(item.get()))
                        pItem->moveAbsoluteStartPosition(diff, context);
                }
            }
            break;
        case rightEdge:
            // move length
            diff = newRange.getLength() - positionableBase.getRegionData(context).getLength();
            if (std::abs(diff) > 0.0) {
                for (auto item : selectedItems) {
                    if (auto pItem = dynamic_cast<PositionableBase*>(item.get()))
                        pItem->moveLength(diff, context);
                }
            }
            break;
        case middleEdge:
            // move position
            diff = newRange.getStart() - positionableBase.getAbsolutePosition(context);
            if (std::abs(diff) > 0.0) {
                for (auto item : selectedItems) {
                    if (auto pItem = dynamic_cast<PositionableBase*>(item.get()))
                        pItem->moveAbsolutePosition(diff, context);
                }
            }
            break;
        default:
            break;
    }
    
    if (std::abs(diff) > 0.0)
    {
        audiumEngine->getAudioTrackContainer()->sendActionMessage(updateArrangementAction);
        return true;
    }
    
    return false;
}

void DraggerControl::setComponentToDrag(juce::Component* comp)
{
    componentToDrag = comp;
}

void DraggerControl::setPositionableObject(std::shared_ptr<PositionableBase> object)
{
    positionableObject = object;
}
