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

#include <JuceHeader.h>
#include "FadeInOutControl.h"
#include "Interface/Controls/DraggerControl.h"

void FadeInOutControl::paint (juce::Graphics& g)
{
    // draw outline
    
    g.setColour(isMouseOver() ? juce::Colours::orange : juce::Colours::white.withAlpha(0.5f));
    
    auto bounds = getLocalBounds();
    
    if (type == FadeIn) {
        bounds.setWidth(visualSize);
        bounds.setHeight(visualSize);
    }
    else if (type == FadeOut) {
        bounds.setX(getWidth() - visualSize);
        bounds.setWidth(visualSize);
        bounds.setHeight(visualSize);
    }
    g.drawRect (bounds, 1);
}

void FadeInOutControl::resized()
{
}


void FadeInOutControl::mouseDown (const juce::MouseEvent& e)
{
    originalBounds = getBounds();
    NullCheckedInvocation::invoke(onDragStart);
}

void FadeInOutControl::mouseUp (const juce::MouseEvent& e)
{
    NullCheckedInvocation::invoke(onDragEnd);
}

void FadeInOutControl::mouseDrag (const juce::MouseEvent& e)
{
    auto distance = e.getOffsetFromDragStart();
    distance.setY(0); // drag vertically only
    
    if (std::abs(distance.getX()) > 0) {
        auto newBounds = originalBounds + distance;
        
        auto leftLimit = (type == FadeIn) ? 0 : controlWidth;
        auto rightLimit = (type == FadeIn) ? (getParentWidth() - (controlWidth * 2)) : (getParentWidth() - controlWidth);
        
        if (newBounds.getX() < leftLimit)
            newBounds.setX(leftLimit);
        
        if (newBounds.getX() > rightLimit)
            newBounds.setX(rightLimit);
        
        setBounds (newBounds);
        
        NullCheckedInvocation::invoke(onValueChange);
    }
}

void FadeInOutControl::mouseMove (const juce::MouseEvent& e)
{
    //updateMouseZone (e);
}

void FadeInOutControl::mouseEnter (const MouseEvent& e)
{
    if (regionSelector != nullptr)
        regionSelector->setEnabled(false);
    
    repaint();
}

void FadeInOutControl::mouseExit (const MouseEvent& e)
{
    getParentComponent()->mouseExit(e);
    
    if (regionSelector != nullptr)
        regionSelector->setEnabled(true);
    
    repaint();
}

double FadeInOutControl::getValue() const
{
    auto totalWidth = static_cast<double>(getParentWidth() - controlWidth);
    auto xPos = getBounds().toDouble().getX();
    
    if (type == FadeIn) {
        return xPos / totalWidth;
    }
    else if (type == FadeOut) {
        return (totalWidth - xPos) / totalWidth;
    }
    
    return 0.0;
}

void FadeInOutControl::setValue(double val)
{
    juce::jlimit(0.0, 1.0, val);
    auto totalWidth = static_cast<double>(getParentWidth() - controlWidth);
    
    if (type == FadeIn) {
        auto x = static_cast<int>(totalWidth * val);
        setBounds(x, DraggerControl::draggerHeight + 1, controlWidth, controlHeight);
    }
    else if (type == FadeOut) {
        auto x = static_cast<int>(totalWidth - (totalWidth * val));
        setBounds(x, DraggerControl::draggerHeight + 1, controlWidth, controlHeight);
    }
}
