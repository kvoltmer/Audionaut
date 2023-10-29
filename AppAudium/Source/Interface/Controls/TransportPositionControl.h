/*
  ==============================================================================

    TransportPositionControl.h
    Created: 21 Jun 2023 2:38:05pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/Views/TransportView.h"

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
        transportView.reset(new TransportView(zoomHandler));
        addAndMakeVisible(transportView.get());
        transportView->toBack();
        
        currentPositionMarker.setFill (Colours::white);
        addAndMakeVisible (currentPositionMarker);
        
        mouseOverMarker.setFill (Colours::red.withAlpha (0.85f));
        addAndMakeVisible (mouseOverMarker);
        
        startTimerHz (40);
    }

    ~TransportPositionControl() override
    {
    }

    void paint (juce::Graphics& g) override
    {
    }

    void resized() override
    {
        transportView->setBounds(getBounds());
    }
    
    void timerCallback() override
    {
        auto pos = 0.0;
        auto playListScheduler = audiumEngine->getPlayListScheduler();
        if (playListScheduler != nullptr)
        {
            pos = playListScheduler->getAbsolutePosition();
        }
        
        currentPositionMarker.setRectangle (Rectangle<float> (zoomHandler->timeToX(pos) - 0.75f, 0,
                                                              1.5f, (float) getHeight()));
    
        if (playListScheduler->isPlaying())
        {
            currentPositionMarker.setVisible(false);
        }
        else
        {
            counter++;
            if (counter > 20)
            {
                // toggle
                currentPositionMarker.setVisible( !currentPositionMarker.isVisible());
                counter = 0;
            }
        }
    }
    
    
    void mouseDown (const MouseEvent& e) override
    {
        mouseDrag(e);

    }
    
    void mouseDrag (const MouseEvent& e) override
    {
        // auto x1 = e.getPosition().getX();
        auto relativeEvent = e.getEventRelativeTo(owner.get());
        auto x = relativeEvent.getPosition().getX();
        auto pos = zoomHandler->xToTimeWithOffset(x);
        // std::cout << x1 << " " << x << " " << pos << std::endl;

        // set transport position
        audiumEngine->getPlayListScheduler()->setAbsolutePosition (pos);
    }
    
    void mouseMove (const MouseEvent& e) override
    {
        mouseOverMarker.setRectangle (Rectangle<float> (e.getPosition().getX() - 0.75f, 0,
                                                              1.5f, (float) getHeight()));
    }
    
    void mouseEnter (const MouseEvent&) override
    {
        mouseOverMarker.setVisible(true);
    }
    void mouseExit (const MouseEvent&) override
    {
        mouseOverMarker.setVisible(false);
    }

private:
    /// TODO: maybe viewport is sufficiant?
    std::shared_ptr<audium::ListBox> owner;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    juce::DrawableRectangle currentPositionMarker;
    juce::DrawableRectangle mouseOverMarker;
    
    std::unique_ptr<TransportView> transportView;
    
    int counter = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportPositionControl)
};
