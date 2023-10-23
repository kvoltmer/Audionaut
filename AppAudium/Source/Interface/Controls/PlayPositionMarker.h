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

//==============================================================================
/*
*/

class AudiumEngine;


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

    void paint (juce::Graphics&) override
    {
    }

    void resized() override
    {
    }
    
    void timerCallback() override
    {
        updateCursorPosition();
    }
    
    void updateCursorPosition()
    {
        if (audiumEngine->getPlayListScheduler() != nullptr)
        {
            if (audiumEngine->getPlayListScheduler()->isPlaying())
            {
                currentPositionMarker.setVisible(true);
                auto pos = audiumEngine->getPlayListScheduler()->getAbsolutePosition();
                auto xPos = zoomHandler->timeToXWithOffset(pos);
                currentPositionMarker.setRectangle (Rectangle<float> (xPos - 0.75f, 0,
                                                                      1.5f, (float) (getHeight() - zoomHandler->getScrollBarHeight())));
            }
            else
            {
                currentPositionMarker.setVisible(false);
            }
        }
    }
    
private:
        
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    juce::DrawableRectangle currentPositionMarker;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayPositionMarker)
};
