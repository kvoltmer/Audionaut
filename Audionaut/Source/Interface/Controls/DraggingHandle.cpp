//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <algorithm>

#include <JuceHeader.h>
#include "DraggingHandle.h"
#include "Interface/Controls/DraggerControl.h"

void DraggingHandle::paint (juce::Graphics& g)
{
    // draw outline
    
    g.setColour(isMouseOver() ? juce::Colours::orange : juce::Colours::white.withAlpha(0.5f));

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
    distance.setY(0); // drag vertically only
    
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
