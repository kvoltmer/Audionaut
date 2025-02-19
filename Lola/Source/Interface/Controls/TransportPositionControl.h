/*
  ==============================================================================

    TransportPositionControl.h
    Created: 21 Jun 2023 2:38:05pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/PlayList/TransportLoop.h"

#include "Interface/Views/TransportView.h"
#include "Interface/Controls/LoopDraggerControl.h"

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
  

        auto gridColour = findColour (audium::gridColourId);
        
        transportView = std::make_unique<TransportView> (zoomHandler);
        addAndMakeVisible(transportView.get());

        loopRangeMarker.setFill(Colours::white.withAlpha(0.1f));
        loopRangeMarker.setStrokeFill(Colours::white.withAlpha(0.85f));
        loopRangeMarker.setStrokeThickness(1.f);

        addAndMakeVisible(loopRangeMarker);
        
        startPositionMarker.setFill (Colours::red.withAlpha (0.85f));
        addAndMakeVisible (startPositionMarker);
        
        mouseOverGridMarker.setFill(gridColour);
        addAndMakeVisible(mouseOverGridMarker);
        

        loopDraggerControl = std::make_unique<LoopDraggerControl>(nullptr,
                                                                  audiumEngine,
                                                                  zoomHandler,
                                                                  gridColour,
                                                                  regionSelector);
        addAndMakeVisible(loopDraggerControl.get());
        
        loopDraggerControl->onDragEnd = [this] () {
            auto transportLoop = audiumEngine->getPlayListScheduler()->getTransportLoop();
            transportLoop->setLoopPositionRange(audiumEngine->getAudioTrackContainer(),
                                                loopDraggerControl->loopRangeInClocks,
                                                audium::clocks);
            updateLoopView();
            
        };
        
        
        startTimerHz (40);
        
        updateLoopView();
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
        updateLoopView();
    }
    
    void timerCallback() override
    {
        if (auto playListScheduler = audiumEngine->getPlayListScheduler()) {
            auto position = zoomHandler->clocksToX(playListScheduler->getAbsolutePosition(audium::clocks));
            auto start = zoomHandler->clocksToX(playListScheduler->getAbsoluteStartPosition(audium::clocks));
            auto height = static_cast<float>(getHeight());
            startPositionMarker.setRectangle (Rectangle<float> (start - 0.75f, 0.f,
                                                                  1.5f, height));
            startPositionMarker.setVisible(std::abs(position - start) > 0.0);
            
            auto loopActive = playListScheduler->getTransportLoop()->isLoopActive();
            loopRangeMarker.setVisible(loopActive);
            loopDraggerControl->setVisible(loopActive);
        }
            
    }
    
    void updateLoopView()
    {
        if (auto playListScheduler = audiumEngine->getPlayListScheduler()) {
            
            auto loopActive = playListScheduler->getTransportLoop()->isLoopActive();

            if (loopActive) {
            
                auto height = static_cast<float>(getHeight());
                auto loopRange = playListScheduler->getTransportLoop()->getLoopPositionRange(audium::clocks);
                auto loopX = zoomHandler->clocksToX(loopRange);
                
                Rectangle<float> loopRect (loopX.getStart(), 0.f, loopX.getLength(), height);
                
                loopRangeMarker.setRectangle(loopRect);
                
                loopDraggerControl->setBounds(loopRect.toNearestInt());
            }
        }
    }
    
    
    void mouseUp (const MouseEvent& e) override
    {
        //std::cout << "TransportPositionControl::mouseUp" << std::endl;
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
                audiumEngine->getPlayListScheduler()->setAbsoluteStartPosition(clocks, audium::clocks);
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
    
            auto gridX = zoomHandler->clocksToX(clocks);
            
            mouseOverGridMarker.setRectangle (Rectangle<float> (gridX - 0.75f, 0, 1.5f, (float) getHeight()));
        }
        else {
            mouseOverGridMarker.setVisible(false);
        }
    }
    
    void mouseEnter (const MouseEvent&) override
    {
        mouseOverGridMarker.setVisible(true);
    }
    void mouseExit (const MouseEvent&) override
    {
        mouseOverGridMarker.setVisible(false);
    }

private:

    std::shared_ptr<audium::ListBox> owner;
    std::shared_ptr<RegionSelector> regionSelector;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    juce::DrawableRectangle startPositionMarker;
    juce::DrawableRectangle mouseOverGridMarker;
    juce::DrawableRectangle loopRangeMarker;
    
    std::unique_ptr<TransportView> transportView;
    std::unique_ptr<LoopDraggerControl> loopDraggerControl;
    
    int counter = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportPositionControl)
};
