
#pragma once

#include <JuceHeader.h>

#include "Interface/Handlers/ZoomHandler.h"

class PositionMarker  : public juce::Component,
                            private juce::Timer
{
public:
    PositionMarker(std::shared_ptr<ZoomHandler> zoomHandler) :
        zoomHandler(zoomHandler)
    {
        setColour(Colours::pink);
        addAndMakeVisible (currentPositionMarker);
        
        startTimerHz (60);
        
        setInterceptsMouseClicks(false, false);
    }

    ~PositionMarker() override
    {
        stopTimer();
    }
    
    void timerCallback() override
    {
        if (getParentComponent()->isVisible())
            updateCursorPosition();
    }
    
    void setColour(juce::Colour colour)
    {
        currentPositionMarker.setFill (colour);
    }
    
    void updateCursorPosition()
    {
        if (onUpdatePosition != nullptr) {
            auto position = onUpdatePosition(audium::seconds);
            auto xPos = zoomHandler->secondsToXWithOffset(position);
            auto iPos = static_cast<int>(xPos);
            if (iPos != lastXPos) {
                lastXPos = iPos;
                
                if (xPos >= 0.0) {
                    currentPositionMarker.setVisible(true);
                    currentPositionMarker.setRectangle (Rectangle<float> (xPos - 0.75f, 0,
                                                                          1.5f, (float) (getHeight() - zoomHandler->getScrollBarHeight())));
                }
                else {
                    currentPositionMarker.setVisible(false);
                }
            }
        }
    }
    
    std::function<double(const audium::TimeContextType context)> onUpdatePosition = nullptr;
    
private:
        
    std::shared_ptr<ZoomHandler> zoomHandler;
    
    juce::DrawableRectangle currentPositionMarker;
    
    int lastXPos = -1;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PositionMarker)
};
