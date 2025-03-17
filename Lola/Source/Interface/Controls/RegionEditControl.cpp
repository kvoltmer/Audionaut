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

#include <iostream>

#include "RegionEditControl.h"

#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioSubGroup.h"
#include "Engine/Group/AudioClip.h"

#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Handlers/SnapToGridHandler.h"
#include "Interface/Controls/RegionSelector.h"
#include "Interface/ColourIds.h"

void RegionEditControl::paintFileNameLabel (juce::Graphics& g)
{
    g.setFont (12.0f);
    const auto name = audioRegion->getName();
    juce::Rectangle<int> bonds(5,
                               5,
                               GlyphArrangement::getStringWidth (g.getCurrentFont(), name),
                               g.getCurrentFont().getHeight());
    
    g.setColour(findColour(audium::secondaryBackgroundColourId));
    g.fillRoundedRectangle (bonds.expanded(2, 2).toFloat(), 3.0f);
    
    g.setColour (findColour(audium::defaultTextColourId));
    g.drawFittedText (name, bonds, juce::Justification::topLeft, 1);
}

void RegionEditControl::paint (Graphics& g)
{
    auto thumbArea = getLocalBounds();
    auto colour = Colours::white;
    
    // rectangle
    if (audioRegion->isSelected())
    {
        g.setColour (colour.withAlpha(1.f));
    }
    else
    {
        g.setColour (colour.withAlpha(0.5f));
    }
    g.drawRoundedRectangle (thumbArea.toFloat(), 3.0f, 1.5f);
    
    // fill
    g.setColour (colour.withAlpha(0.125f));
    g.fillRoundedRectangle (thumbArea.toFloat(), 3.0f);
    
    paintFileNameLabel(g);
}

void RegionEditControl::mouseDown (const juce::MouseEvent& e)
{
    // std::cout << "RegionEditControl::mouseDown" << std::endl;
    regionSelector->setEnabled(false);
    
    // TODO: shiftSelect
    bool deselectOthers = !audioRegion->isSelected() && !e.mods.isAnyModifierKeyDown();
    audioRegion->setSelected(e.mods.isCommandDown() ? !audioRegion->isSelected() : true, deselectOthers);
    
    audiumEngine->getAudioTrackContainer()->sendActionMessage(audium::updateSelection);

    currentDragMode = getDragMode(e.getPosition().getX());
    
    originalBounds = getBounds();
}

void RegionEditControl::mouseDrag (const juce::MouseEvent& e)
{
    
    auto distance = e.getOffsetFromDragStart();
    distance.setY(0); // drag vertically only
    
    auto bounds = originalBounds;
    
    switch (currentDragMode) {
        case RegionEditControl::leftEdge:
            //bounds.setLeft(juce::jmin(originalBounds.getRight() - minimumWidth, originalBounds.getX() + distance.getX()));
            bounds.setLeft(originalBounds.getX() + distance.getX());
            break;
        case RegionEditControl::rightEdge:
            bounds.setRight(juce::jmax(originalBounds.getX() + minimumWidth, originalBounds.getRight() + distance.getX()));
            break;
        case RegionEditControl::middleEdge:
            bounds += distance;
            break;
        default:
            jassertfalse;
            break;
    }
    
    setBounds (bounds);
    
    auto audioClipStart = audioRegion->getAudioSubGroup()->getAbsolutePosition(audium::clocks);
    auto rangeInClocks =   zoomHandler->xToClocks(getBounds().toDouble().getHorizontalRange()) + audioClipStart;
    zoomHandler->getSnapToGridHandler()->publishRange(rangeInClocks);
    repaint();
    
}

void RegionEditControl::mouseUp (const juce::MouseEvent& e)
{
    regionSelector->setEnabled(true);
    if (e.getPosition() != e.getMouseDownPosition())
    {
        
        // Undo: store old state
        auto action = std::make_unique<audium::UndoableContainerAction>(*audiumEngine->getAudioTrackContainer(), false);
        
        auto rangeInClocks =   zoomHandler->xToClocks(getBounds().toDouble().getHorizontalRange());
        
        auto audioClipStart = audioRegion->getAudioSubGroup()->getAbsolutePosition(audium::clocks);

        rangeInClocks += audioClipStart;
        zoomHandler->snapToGrid(rangeInClocks);
        rangeInClocks -= audioClipStart;

        // note: add audio resource start (getAudioResourceStart)
        rangeInClocks += audioRegion->getAudioResourceStart(audium::clocks);
        
        audioRegion->validateData(rangeInClocks, audium::clocks);
        // apply to selected regions
        auto selectedItems = audiumEngine->getAudioTrackContainer()->getSelectionManager()->getSelectedObjects();
        auto startDiff = rangeInClocks.getStart() - audioRegion->getRegionData(audium::clocks).getStart();
        auto endDiff = rangeInClocks.getEnd() - audioRegion->getRegionData(audium::clocks).getEnd();
        for (auto item : selectedItems) {
            if (auto region = dynamic_cast<audium::AudioRegion*>(item.get())) {
                auto range = region->getRegionData(audium::clocks);
                range.setStart(range.getStart() + startDiff);
                range.setEnd(range.getEnd() + endDiff);
                region->setRegionData(range, audium::clocks);
            }
        }
        audioRegion->setRegionData(rangeInClocks, audium::clocks);
        
        zoomHandler->getSnapToGridHandler()->clearRange();
        
        // Undo: store new state
        action->storeNewState();
        audiumEngine->getUndoManager()->perform(action.release(), "Move Region");
        audiumEngine->getUndoManager()->beginNewTransaction();
        
    }
}

void RegionEditControl::mouseMove (const juce::MouseEvent& e)
{
    updateMouseZone (e);
}

void RegionEditControl::updateFromEngine(std::shared_ptr<audium::AudioRegion> newRegion)
{
    jassert(newRegion != nullptr);
    if (newRegion != audioRegion)
    {
        audioRegion = newRegion;
    }
    auto bounds = getBounds().toFloat();
    
    // note: subtract audio resource start
    auto rangeSeconds = audioRegion->getRegionData(audium::seconds) - audioRegion->getAudioResourceStart(audium::seconds);
    auto rangeX = zoomHandler->secondsToX(rangeSeconds);
    bounds.setX(rangeX.getStart());
    bounds.setWidth(rangeX.getLength());
    
    setBounds(bounds.toNearestInt());
    
}

void RegionEditControl::updateMouseZone (const juce::MouseEvent& e)
{
    //std::cout << "RegionEditControl::updateMouseZone" << std::endl;
    
    switch (getDragMode(e.getPosition().getX())) {
        case RegionEditControl::leftEdge:
            setMouseCursor (MouseCursor::LeftEdgeResizeCursor);
            break;
        case RegionEditControl::rightEdge:
            setMouseCursor (MouseCursor::RightEdgeResizeCursor);
            break;
        case RegionEditControl::middleEdge:
            setMouseCursor (MouseCursor::DraggingHandCursor);
            break;
        default:
            break;
    }
}

const RegionEditControl::Edge RegionEditControl::getDragMode(int x) const
{
    if (x < borderSize)
    {
        return RegionEditControl::leftEdge;
    }
    else if (getWidth() - x < borderSize)
    {
        return RegionEditControl::rightEdge;
    }
    else
    {
        return RegionEditControl::middleEdge;
    }
}

bool RegionEditControl::keyPressed (const KeyPress& key, Component* originatingComponent)
{
    if (key.isKeyCode (KeyPress::deleteKey) || key.isKeyCode (KeyPress::backspaceKey))
    {
        audiumEngine->getAudioTrackContainer()->deleteSelectedObjects();
        return true;
    }
    
    return false;
}
