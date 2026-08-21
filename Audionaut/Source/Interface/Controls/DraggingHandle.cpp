//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <algorithm>
#include <cmath>

#include <JuceHeader.h>
#include "DraggingHandle.h"
#include "Interface/Controls/DraggerControl.h"

void DraggingHandle::paint (juce::Graphics& g)
{
    // draw outline

    g.setColour(isMouseOver() ? juce::Colours::orange : juce::Colours::white.withAlpha(0.5f));

    if (isCurveType()) {
        // the resting node is drawn as part of the curve (FadeInOutView /
        // ClipFadeOverlay); this component only highlights hover and drag
        if (isMouseOver() || isMouseButtonDown()) {
            g.setColour(juce::Colours::orange);
            auto circle = getLocalBounds().withSizeKeepingCentre(visualSize, visualSize).toFloat();
            g.drawEllipse(circle.reduced(1.f), 2.f);
        }
        return;
    }

    auto bounds = getLocalBounds();
    bounds.setWidth(visualSize);
    bounds.setHeight(visualSize);

    if (type == FadeOut || type == FadeOutEnd) {
        bounds.setX(getWidth() - visualSize);
    }

    if (type == FadeInStart || type == FadeOutEnd) {
        bounds.setY(getHeight() - visualSize);
    }

    g.drawRect (bounds, 1);
}

void DraggingHandle::resized()
{
}


void DraggingHandle::mouseDown (const juce::MouseEvent& e)
{
    originalBounds = getBounds();
    NullCheckedInvocation::invoke(onDragStart);
}

void DraggingHandle::mouseUp (const juce::MouseEvent& e)
{
    NullCheckedInvocation::invoke(onDragEnd);
}

void DraggingHandle::mouseDrag (const juce::MouseEvent& e)
{
    auto distance = e.getOffsetFromDragStart();

    if (isCurveType()) {
        // the bend handle drags vertically within the channel band
        distance.setX(0);

        if (std::abs(distance.getY()) > 0) {
            auto newBounds = originalBounds + distance;

            auto topLimit = DraggerControl::draggerHeight;
            auto bottomLimit = (bottomAnchorY > 0 ? bottomAnchorY : getParentHeight()) - controlHeight;
            newBounds.setY(juce::jlimit(topLimit, bottomLimit, newBounds.getY()));

            setBounds(newBounds);
            NullCheckedInvocation::invoke(onValueChange);
        }
        return;
    }

    distance.setY(0); // drag horizontally only

    if (std::abs(distance.getX()) > 0) {
        auto newBounds = originalBounds + distance;
        
        auto fromLeft = (type == FadeIn || type == FadeInStart);
        auto leftLimit = fromLeft ? 0 : controlWidth;
        auto rightLimit = fromLeft ? (getParentWidth() - (controlWidth * 2)) : (getParentWidth() - controlWidth);

        if (! clipRange.isEmpty()) {
            // lane-parented: the drag may leave the clip, only the lane
            // itself bounds it
            leftLimit = 0;
            rightLimit = getParentWidth() - controlWidth;
        }
        
        if (newBounds.getX() < leftLimit)
            newBounds.setX(leftLimit);
        
        if (newBounds.getX() > rightLimit)
            newBounds.setX(rightLimit);
        
        setBounds (newBounds);
        
        NullCheckedInvocation::invoke(onValueChange);
    }
}

void DraggingHandle::mouseMove (const juce::MouseEvent& e)
{
    //updateMouseZone (e);
}

void DraggingHandle::mouseEnter (const MouseEvent& e)
{
    if (regionSelector != nullptr)
        regionSelector->setEnabled(false);
    
    repaint();
}

void DraggingHandle::mouseExit (const MouseEvent& e)
{
    // notify the clip component, not the parent - lane-parented handles
    // no longer have the clip as their parent
    if (exitTarget != nullptr)
        exitTarget->mouseExit(e);
    else if (getParentComponent() != nullptr)
        getParentComponent()->mouseExit(e);

    if (regionSelector != nullptr)
        regionSelector->setEnabled(true);

    repaint();
}

double DraggingHandle::getValue() const
{
    if (isCurveType()) {
        // value = the curve exponent p: the handle's centre marks the curve
        // value at the ramp midpoint, 0.5^p of the band height
        auto bandBottom = static_cast<double>(bottomAnchorY > 0 ? bottomAnchorY : getParentHeight());
        auto bandHeight = bandBottom - DraggerControl::draggerHeight;
        if (bandHeight <= 0.0)
            return audium::ClipDynamics::defaultFadeCurve;

        auto midValue = (bandBottom - getBounds().toDouble().getCentreY()) / bandHeight;
        midValue = juce::jlimit(0.03, 0.97, midValue);
        return std::log(midValue) / std::log(0.5);
    }

    // clipRange set: the handle lives in the lane, values map against the
    // clip's x-range and go negative outside it
    auto clipX = clipRange.isEmpty() ? 0.0 : static_cast<double>(clipRange.getStart());
    auto clipWidth = clipRange.isEmpty() ? static_cast<double>(getParentWidth())
                                         : static_cast<double>(clipRange.getLength());
    auto totalWidth = clipWidth - controlWidth;
    auto xPos = getBounds().toDouble().getX() - clipX;

    if (type == FadeIn || type == FadeInStart) {
        return xPos / totalWidth;
    }
    else if (type == FadeOut || type == FadeOutEnd) {
        return (totalWidth - xPos) / totalWidth;
    }

    return 0.0;
}

void DraggingHandle::setValue(double val)
{
    if (isCurveType()) {
        // val = the curve exponent p; sit on the ramp midpoint (clipRange
        // holds the RAMP's x-range in lane coords) at 0.5^p band height
        auto bandBottom = bottomAnchorY > 0 ? bottomAnchorY : getParentHeight();
        auto bandTop = DraggerControl::draggerHeight;
        auto bandHeight = static_cast<double>(bandBottom - bandTop);

        auto midValue = std::pow(0.5, val);
        auto x = clipRange.getStart() + clipRange.getLength() / 2 - controlWidth / 2;
        auto y = static_cast<int>(bandBottom - midValue * bandHeight) - controlHeight / 2;
        y = juce::jlimit(bandTop, bandBottom - controlHeight, y);
        setBounds(x, y, controlWidth, controlHeight);
        return;
    }

    auto fromLeft = (type == FadeIn || type == FadeInStart);
    auto onBottom = (type == FadeInStart || type == FadeOutEnd);

    // the bottom handles may go negative (outside the clip); the model owns
    // the <= 1 bound
    val = onBottom ? std::min(val, 1.0) : juce::jlimit(0.0, 1.0, val);

    auto clipX = clipRange.isEmpty() ? 0 : clipRange.getStart();
    auto clipWidth = clipRange.isEmpty() ? getParentWidth() : clipRange.getLength();
    auto totalWidth = static_cast<double>(clipWidth - controlWidth);

    auto x = clipX + (fromLeft ? static_cast<int>(totalWidth * val)
                               : static_cast<int>(totalWidth - (totalWidth * val)));
    auto bottomY = bottomAnchorY > 0 ? bottomAnchorY : getParentHeight();
    auto y = onBottom ? bottomY - controlHeight - 1
                      : DraggerControl::draggerHeight + 1;
    setBounds(x, y, controlWidth, controlHeight);
}
