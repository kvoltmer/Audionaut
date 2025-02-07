
#include <JuceHeader.h>
#include "FadeInOutControl.h"

void FadeInOutControl::paint (juce::Graphics& g)
{
    // draw outline
    
    g.setColour(isMouseOver() ? juce::Colours::orange : juce::Colours::white.withAlpha(0.5f));
    
    auto bounds = getLocalBounds();
    auto visualSize = 10;
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
        
        if (newBounds.getX() < 0)
            newBounds.setX(0);
        
        if ((newBounds.getX() + originalBounds.getWidth())  > getParentWidth())
            newBounds.setX(getParentWidth() - originalBounds.getWidth());
        
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
    auto totalWidth = static_cast<double>(getParentWidth() - getWidth());
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
    auto totalWidth = static_cast<double>(getParentWidth() - getWidth());
    
    auto bounds = getLocalBounds();
    
    if (type == FadeIn) {
        auto x = static_cast<int>(totalWidth * val);
        setBounds(x, bounds.getY(), bounds.getWidth(), bounds.getHeight());
    }
    else if (type == FadeOut) {
        auto x = static_cast<int>(totalWidth - (totalWidth * val));
        setBounds(x, bounds.getY(), bounds.getWidth(), bounds.getHeight());
    }
    
    jassert(getValue() == val);
}
