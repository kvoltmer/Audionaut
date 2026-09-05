//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Interface/Controls/DraggerControl.h"
#include "Interface/Components/MiddlePanel/ArrangementView/PlayListItemComponent.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Selection/ClipOverlayTarget.h"

void DraggerControl::mouseDown (const juce::MouseEvent& e)
{
    
    currentDragMode = getDragMode(e.getPosition().getX());
    stretchDrag = (isStretchModifier(e.mods) || overlayStretchActive())
                  && (currentDragMode == leftEdge || currentDragMode == rightEdge);
    
    originalBounds = componentToDrag->getBounds();
    
    if (e.mods.isShiftDown()) {
        shiftSelect();
    }
    else if (stretchDrag) {
        // a stretch acts on the selection: make sure this clip is in it
        // without toggling anything (on Windows Ctrl doubles as Command)
        setSelected(true, !isSelected());
    }
    else {
        bool deselectOthers = !isSelected() && !e.mods.isAnyModifierKeyDown();
        setSelected(e.mods.isCommandDown() ? !isSelected() : true, deselectOthers);
    }
    auto rangeInClocks = zoomHandler->xToClocks(componentToDrag->getBounds().toDouble().getHorizontalRange());
    zoomHandler->getSnapToGridHandler()->publishRange(rangeInClocks, true);
    
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
            audiumEngine->getUndoManager()->perform(undoableContainerAction.release(),
                                                    stretchDrag ? "Stretch Clip" : "Modify Item");
            audiumEngine->getUndoManager()->beginNewTransaction();
        }
        sendChangeMessage();
    }

    stretchDrag = false;
    zoomHandler->getSnapToGridHandler()->clearRange();
}

void DraggerControl::mouseDrag (const juce::MouseEvent& e)
{
    if (isRecording()) {
        // no dragging while recording!
        return;
    }
    
    auto container = juce::DragAndDropContainer::findParentDragContainerFor(componentToDrag);
    auto dragActive = (container != nullptr && container->isDragAndDropActive());
    auto dragVertrically = (std::abs(e.getDistanceFromDragStartY()) > 20 && std::abs(e.getDistanceFromDragStartX()) < 20);
    
    beginDragAutoRepeat(40);
    autoScrollOffset += zoomHandler->autoScroll(e);

    if (!stretchDrag &&
        (e.mods.isAltDown() || dragVertrically)) {

        if (container != nullptr) {
            // componentToDrag has PlayListItemComponent as a child.
            // see: dragger->setComponentToDrag(getParentComponent());
            for (auto* child : componentToDrag->getChildren()) {
                if (auto playListComp = dynamic_cast<PlayListItemComponent*> (child)) {
                    container->startDragging("DraggerControl", playListComp);
                    break;
                }
            }
        }
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
            zoomHandler->getSnapToGridHandler()->publishRange(rangeInClocks, true);
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
        audiumEngine->getAudioTrackContainer()->sendActionMessage(audium::updateArrangementAction);
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

bool DraggerControl::commitPositionData(const audium::PositionableBase &positionableBase,
                                        const juce::Range<double> newRange,
                                        const audium::TimeContextType context)
{
    // apply for all selected items
    auto selectedItems = audiumEngine->getAudioTrackContainer()->getSelectionManager()->getSelectedObjects();
    auto diff = 0.0;
    
    if (stretchDrag && (currentDragMode == leftEdge || currentDragMode == rightEdge))
    {
        // stretch: keep every clip's source window and derive the speed
        // from the dragged timeline length; all selected clips take the
        // same resulting ratio
        const auto newLength = newRange.getLength();
        const auto anchorSource = positionableBase.getRegionData(context).getLength();

        if (newLength <= 0.0 || anchorSource <= 0.0)
            return false;

        const auto ratio = anchorSource / newLength;
        auto changed = false;

        for (auto item : selectedItems) {
            if (auto playItem = dynamic_cast<audium::PlayListItem*>(item.get())) {
                const auto oldRange = playItem->getAbsolutePositionRange(context);
                playItem->setSpeedRatio(ratio);

                // a left-edge stretch keeps the right edge anchored
                if (currentDragMode == leftEdge)
                    playItem->setAbsolutePosition(oldRange.getEnd()
                                                  - playItem->getAbsolutePositionRange(context).getLength(),
                                                  context);
                changed = true;
            }
        }

        if (changed)
            audiumEngine->getAudioTrackContainer()->sendActionMessage(audium::updateArrangementAction);

        return changed;
    }

    switch (currentDragMode)
    {
        case leftEdge:
            diff = newRange.getStart() - positionableBase.getAbsolutePosition(context);
            if (std::abs(diff) > 0.0) {
                for (auto item : selectedItems) {
                    if (auto pItem = dynamic_cast<audium::PositionableBase*>(item.get()))
                        pItem->moveAbsoluteStartPosition(diff, context);
                }
            }
            break;
        case rightEdge:
            // move timeline length (a re-pitched clip's extent differs from
            // its source window)
            diff = newRange.getLength() - positionableBase.getAbsolutePositionRange(context).getLength();
            if (std::abs(diff) > 0.0) {
                for (auto item : selectedItems) {
                    if (auto pItem = dynamic_cast<audium::PositionableBase*>(item.get()))
                        pItem->moveLength(diff, context);
                }
            }
            break;
        case middleEdge:
            // move position
            diff = newRange.getStart() - positionableBase.getAbsolutePosition(context);
            if (std::abs(diff) > 0.0) {
                for (auto item : selectedItems) {
                    if (auto pItem = dynamic_cast<audium::PositionableBase*>(item.get()))
                        pItem->moveAbsolutePosition(diff, context);
                }
            }
            break;
        default:
            break;
    }
    
    if (std::abs(diff) > 0.0)
    {
        audiumEngine->getAudioTrackContainer()->sendActionMessage(audium::updateArrangementAction);
        return true;
    }
    
    return false;
}

bool DraggerControl::overlayStretchActive() const
{
    auto target = audiumEngine->getAudioTrackContainer()->getClipOverlayTarget();

    if (! target->isActive())
        return false;

    // Only the clip the overlay is open on is in stretch mode; the loop
    // dragger and other positionables are unaffected.
    if (auto item = std::dynamic_pointer_cast<audium::PlayListItem>(positionableObject))
        return target->matches(item->getRegion()->getAudioTrack()->getId(), item->getId());

    return false;
}

void DraggerControl::setComponentToDrag(juce::Component* comp)
{
    componentToDrag = comp;
}

void DraggerControl::setPositionableObject(std::shared_ptr<audium::PositionableBase> object)
{
    positionableObject = object;
}
