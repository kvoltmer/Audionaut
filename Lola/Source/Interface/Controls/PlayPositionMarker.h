/*
  ==============================================================================

    PlayPositionMarker.h
    Created: 13 Jun 2023 11:22:39am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/AudiumEngine.h"
#include "Interface/Handlers/ZoomHandler.h"

class PlayPositionMarker  : public juce::Component,
                            private juce::Timer
{
public:
    PlayPositionMarker(std::shared_ptr<ZoomHandler> zoomHandler,
                       std::shared_ptr<AudiumEngine> audiumEngine) :
        zoomHandler(zoomHandler),
        audiumEngine(audiumEngine)
    {
        currentPositionMarker.setFill (Colours::white.withAlpha (0.85f));
        addAndMakeVisible (currentPositionMarker);
        
        startTimerHz (60);
        
        setInterceptsMouseClicks(false, true);
        
    }

    ~PlayPositionMarker() override
    {
    }

    void paint (juce::Graphics& g) override
    {
        //g.fillAll (juce::Colours::red);
    }

    void resized() override
    {
    }
    
    void timerCallback() override
    {
        if (getParentComponent()->isVisible())
            updateCursorPosition();
    }
    
    void updateCursorPosition()
    {
        if (audiumEngine->getPlayListScheduler() != nullptr) {

            auto pos = audiumEngine->getPlayListScheduler()->getAbsolutePosition(audium::seconds);
            auto xPos = zoomHandler->secondsToXWithOffset(pos);
            
            if (static_cast<int>(xPos) != lastXPos) {
                lastXPos = static_cast<int>(xPos);
                
                currentPositionMarker.setVisible(xPos >= 0);
                
                if (xPos >= 0.0) {
                    currentPositionMarker.setRectangle (Rectangle<float> (xPos - 0.75f, 0,
                                                                          1.5f, (float) (getHeight() - zoomHandler->getScrollBarHeight())));
                }
            }
        }
    }
    
private:
        
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    juce::DrawableRectangle currentPositionMarker;
    
    int lastXPos = -1;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayPositionMarker)
};
