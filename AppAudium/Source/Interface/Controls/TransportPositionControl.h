/*
  ==============================================================================

    TransportPositionControl.h
    Created: 21 Jun 2023 2:38:05pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
*/
class TransportPositionControl  :   public juce::Component,
                                    private juce::Timer
{
public:
    TransportPositionControl(std::shared_ptr<audium::ListBox> lb,
                             std::shared_ptr<ZoomHandler> zoomHandler,
                             std::shared_ptr<AudiumEngine> audiumEngine) :
        owner(lb),
        zoomHandler(zoomHandler),
        audiumEngine(audiumEngine)
    {
        currentPositionMarker.setFill (Colours::white.withAlpha (0.85f));
        addAndMakeVisible (currentPositionMarker);
        startTimerHz (40);
    }

    ~TransportPositionControl() override
    {
    }

    void paint (juce::Graphics& g) override
    {
        /* This demo code just fills the component's background and
           draws some placeholder text to get you started.

           You should replace everything in this method with your own
           drawing code..
        */

        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));   // clear the background

//        g.setColour (juce::Colours::grey);
//        g.drawRect (getLocalBounds(), 1);   // draw an outline around the component

        g.setColour (juce::Colours::white);
        g.setFont (14.0f);
        g.drawText ("TransportPositionControl", getLocalBounds(),
                    juce::Justification::centred, true);   // draw some placeholder text
    }

    void resized() override
    {
        // This method is where you should set the bounds of any child
        // components that your component contains..

    }
    
    void timerCallback() override
    {
        auto pos = 0.0;
        auto transportSource = audiumEngine->getAudiumTransportSource();
        if (transportSource != nullptr)
        {
            pos = transportSource->getCurrentPosition();
        }
        //auto point = juce::Point(zoomHandler->timeToXWithOffset(pos) - 0.75f, 0);
        //auto local = getLocalPoint(owner, point);
        /// TODO: fixme
        currentPositionMarker.setRectangle (Rectangle<float> (zoomHandler->timeToXWithOffset(pos) - 0.75f, 0,
                                                              1.5f, (float) getHeight()));
    }
    
    
    void mouseDown (const MouseEvent& e) override
    {
        mouseDrag (e);
    }
    
    void mouseDrag (const MouseEvent& e) override
    {
        // set transport position
        auto relativePos = e.getEventRelativeTo(owner.get()).getPosition();
        auto pos = zoomHandler->xToTimeWithOffset(relativePos.getX());;
        audiumEngine->getAudiumTransportSource()->setPosition (pos);
        
    }

private:
    /// TODO: maybe viewport is sufficiant?
    std::shared_ptr<audium::ListBox> owner;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    juce::DrawableRectangle currentPositionMarker;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportPositionControl)
};
