/*
  ==============================================================================

    PlayPositionMarker.h
    Created: 13 Jun 2023 11:22:39am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumTransportSource.h"

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
        startTimerHz (40);
    }

    ~PlayPositionMarker() override
    {
    }

    void paint (juce::Graphics& g) override
    {
    }

    void resized() override
    {
        auto bonds = getParentComponent()->getBounds();
        setBounds(bonds);
    }
    
    void timerCallback() override
    {
//        if (canMoveTransport())
            updateCursorPosition();
//        else
//        {
//            /// TODO: scrolling
//            jassertfalse;
//            // setRange (visibleRange.movedToStartAt (transportSource.getCurrentPosition() - (visibleRange.getLength() / 2.0)));
//        }
    }
    
    void updateCursorPosition()
    {
        auto pos = 0.0;
        auto transportSource = audiumEngine->getAudiumTransportSource();
        if (transportSource != nullptr)
        {
            currentPositionMarker.setVisible(transportSource->isPlaying());
            pos = transportSource->getCurrentPosition();
        }
        currentPositionMarker.setRectangle (Rectangle<float> (zoomHandler->timeToXWithOffset(pos) - 0.75f, 0,
                                                              1.5f, (float) (getHeight() - zoomHandler->getScrollBarHeight())));
    }
    
    void setFollowsTransport (bool shouldFollow)
    {
        isFollowingTransport = shouldFollow;
    }
    
    void mouseDown (const MouseEvent& e) override
    {
        getParentComponent()->mouseDown(e);
        mouseDrag (e);
    }
    
    void mouseDrag (const MouseEvent& e) override
    {
        getParentComponent()->mouseDrag(e);
    }

private:
    
    bool isFollowingTransport = false;
    
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    juce::DrawableRectangle currentPositionMarker;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayPositionMarker)
};
