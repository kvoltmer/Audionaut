/*
  ==============================================================================

    RegionEditControl.cpp
    Created: 29 Nov 2023 2:09:00pm
    Author:  Klaus Voltmer

  ==============================================================================
*/
#include <iostream>

#include "RegionEditControl.h"

#include "Engine/AudioRegionContainer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/PlayList/PlayListScheduler.h"

#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Controls/RegionSelector.h"
#include "Interface/ColourIds.h"

void RegionEditControl::paintFileNameLabel (juce::Graphics& g)
{
    g.setFont (12.0f);
    const auto name = audioRegion->getName();
    juce::Rectangle<int> bonds(5,
                         5,
                         g.getCurrentFont().getStringWidth(name),
                         g.getCurrentFont().getHeight());
    
    g.setColour(juce::Colours::black.withAlpha(0.25f));
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
    
    if (not audioRegion->isSelected())
    {
        audiumEngine->getAudioRegionContainer()->deselectAll();
        audioRegion->setSelected(true);
        audiumEngine->getAudioRegionContainer()->sendActionMessage(regionSelectedAction);
    }
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
    repaint();
    
}

void RegionEditControl::mouseUp (const juce::MouseEvent& e)
{
    regionSelector->setEnabled(true);
    
    
    Range<double> range(getBounds().getX(), getBounds().getRight());
    
    // note: add audio resource start
    auto rangeInSeconds = zoomHandler->xToSeconds(range) + audioRegion->getAudioResourceStartInSeconds();
    
    // set value in the engine
    audioRegion->validateData(rangeInSeconds);
    audioRegion->setRegionData(rangeInSeconds, audium::seconds);
    
    updateFromEngine();
    
    audiumEngine->getAudioRegionContainer()->sendActionMessage (regionModifiedAction);
}

void RegionEditControl::mouseMove (const juce::MouseEvent& e)
{
    updateMouseZone (e);
}

void RegionEditControl::updateFromEngine()
{
    auto bounds = getBounds().toFloat();
    
    // note: subtract audio resource start
    auto rangeSeconds = audioRegion->getRegionData(audium::seconds) - audioRegion->getAudioResourceStartInSeconds();
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
