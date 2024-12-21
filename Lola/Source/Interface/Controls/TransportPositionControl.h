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
                             std::shared_ptr<RegionSelector> regionSelector_,
                             std::shared_ptr<ZoomHandler> zoomHandler_,
                             std::shared_ptr<AudiumEngine> audiumEngine_) :
        owner(lb),
        regionSelector(regionSelector_),
        zoomHandler(zoomHandler_),
        audiumEngine(audiumEngine_)
    {
        setOpaque(true);
        
        transportView.reset(new TransportView(zoomHandler));
        addAndMakeVisible(transportView.get());
        transportView->toBack();
        
        startPositionMarker.setFill (Colours::white.withAlpha (0.85f));
        addAndMakeVisible (startPositionMarker);
        
        mouseOverMarker.setFill (Colours::red.withAlpha (0.25f));
        addAndMakeVisible (mouseOverMarker);
        
        mouseOverGridMarker.setFill(juce::Colours::red.withAlpha(0.75f));
        addAndMakeVisible(mouseOverGridMarker);
        
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
        transportView->setBounds(getLocalBounds());
    }
    
    void timerCallback() override
    {
        auto clocks = 0.0;
        if (auto playListScheduler = audiumEngine->getPlayListScheduler()) {
            clocks = playListScheduler->getAbsoluteStartPosition(audium::clocks);
        }
        
        startPositionMarker.setRectangle (Rectangle<float> (zoomHandler->clocksToX(clocks) - 0.75f, 0,
                                                              1.5f, (float) getHeight()));
    }
    
    
    void mouseUp (const MouseEvent& e) override
    {
        std::cout << "TransportPositionControl::mouseUp" << std::endl;
        if (regionSelector->isEnabled()) {
            auto selectedRange = audiumEngine->getAudioTrackContainer()->getAudioRegionAdapter().getSelectedRange(audium::clocks);
            auto relativeEvent = e.getEventRelativeTo(owner.get());
            auto y = relativeEvent.getPosition().getY();
            
            if (selectedRange.isEmpty() ||
                y < AudiumLookAndFeel::transportPositionControlHeight) {
                
                auto x = relativeEvent.getPosition().getX();
                //auto seconds = zoomHandler->xToSecondsWithOffset(x);
                auto clocks = zoomHandler->xToClocksWithOffset(x);
                // std::cout << x1 << " " << x << " " << pos << std::endl;
                
                zoomHandler->snapToGrid(clocks);
                
                // set transport position
                audiumEngine->getPlayListScheduler()->setAbsolutePosition(clocks, audium::clocks);
            }
        }
    }
    
    void mouseDown (const MouseEvent& e) override
    {
    }
    
    void mouseDrag (const MouseEvent& e) override
    {
    }
    
    void mouseMove (const MouseEvent& e) override
    {
        auto relativeEvent = e.getEventRelativeTo(owner.get());
        auto x = relativeEvent.getPosition().getX();
        x += zoomHandler->getVisibleRange().getStart();
        
        auto clocks = zoomHandler->xToClocks(x);
        if (zoomHandler->snapToGrid(clocks)) {
            mouseOverGridMarker.setVisible(true);
            mouseOverMarker.setVisible(false);
            
            auto gridX = zoomHandler->clocksToX(clocks);
            
            mouseOverGridMarker.setRectangle (Rectangle<float> (gridX - 0.75f, 0, 1.5f, (float) getHeight()));
        }
        else {
            mouseOverGridMarker.setVisible(false);
            mouseOverMarker.setVisible(true);
        }
        
        mouseOverMarker.setRectangle (Rectangle<float> (x - 0.75f, 0, 1.5f, (float) getHeight()));
    }
    
    void mouseEnter (const MouseEvent&) override
    {
        mouseOverMarker.setVisible(true);
        mouseOverGridMarker.setVisible(true);
    }
    void mouseExit (const MouseEvent&) override
    {
        mouseOverMarker.setVisible(false);
        mouseOverGridMarker.setVisible(false);
    }

private:

    std::shared_ptr<audium::ListBox> owner;
    std::shared_ptr<RegionSelector> regionSelector;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    juce::DrawableRectangle startPositionMarker;
    juce::DrawableRectangle mouseOverMarker;
    juce::DrawableRectangle mouseOverGridMarker;
    
    std::unique_ptr<TransportView> transportView;
    
    int counter = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportPositionControl)
};
