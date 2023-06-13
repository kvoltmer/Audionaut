/*
  ==============================================================================

    PlayPositionMarker.h
    Created: 13 Jun 2023 11:22:39am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

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
        /* This demo code just fills the component's background and
           draws some placeholder text to get you started.

           You should replace everything in this method with your own
           drawing code..
        */

//        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));   // clear the background
//
//        g.setColour (juce::Colours::grey);
//        g.drawRect (getLocalBounds(), 1);   // draw an outline around the component
//
//        g.setColour (juce::Colours::white);
//        g.setFont (14.0f);
//        g.drawText ("PlayPositionMarker", getLocalBounds(),
//                    juce::Justification::centred, true);   // draw some placeholder text
    }

    void resized() override
    {
        // This method is where you should set the bounds of any child
        // components that your component contains..
        auto bonds = getParentComponent()->getBounds();
        setBounds(bonds);

    }
    
    void updateCursorPosition()
    {
        //currentPositionMarker.setVisible (audioResource->getAudioTransportSource()->isPlaying() || isMouseButtonDown());
        auto pos = 0.0;
        if (audiumEngine->getAudioTransportSource() != nullptr)
        {
            pos = audiumEngine->getAudioTransportSource()->getCurrentPosition();
        }
        currentPositionMarker.setRectangle (Rectangle<float> (zoomHandler->timeToX (pos) - 0.75f, 0,
                                                              1.5f, (float) (getHeight() /*  - scrollbar.getHeight()*/)));
        
    
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
        
        if (canMoveTransport())
            audiumEngine->getAudioTransportSource()->setPosition (jmax (0.0, zoomHandler->xToTime ((float) e.x)));
            
    }

private:
    
    bool isFollowingTransport = false;
    
    bool canMoveTransport() const noexcept
    {
        auto tps = audiumEngine->getAudioTransportSource();
        return ! (isFollowingTransport && tps != nullptr && tps->isPlaying());
    }
    
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    juce::DrawableRectangle currentPositionMarker;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayPositionMarker)
};
