/*
  ==============================================================================

    DragZoomControl.h
    Created: 6 Jan 2024 12:26:46pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/PlayList/PlayListItem.h"

class DragZoomControl  : public juce::Component
{
public:
    DragZoomControl(std::shared_ptr<AudiumEngine> audiumEngine,
                    std::shared_ptr<ZoomHandler> zoomHandler,
                    bool arrangementMode) :
        audiumEngine(audiumEngine),
        zoomHandler(zoomHandler),
        arrangementMode(arrangementMode)
    {
        visibileRectangle = std::make_unique<juce::DrawableRectangle>();
        addAndMakeVisible(visibileRectangle.get());
        visibileRectangle->setFill (juce::Colours::transparentBlack);
        visibileRectangle->setStrokeFill (juce::Colours::white);
        visibileRectangle->setStrokeThickness(1.f);
        updateFromEngine();
    }

    ~DragZoomControl() override
    {
    }

    void paint (juce::Graphics& g) override
    {
    }
    

    void resized() override
    {
        updateFromEngine();
    }
    
    void updateFromEngine()
    {
        auto bounds = getLocalBounds().toFloat();
        
        auto contentWidth = zoomHandler->getContentWidth();
        jassert(contentWidth > 0.0);
        auto visibleRange = zoomHandler->getVisibleRange();
        
        auto x = (visibleRange.getStart() / contentWidth) * bounds.getWidth();
        auto y = bounds.getY();
        auto w = (visibleRange.getLength() / contentWidth) * bounds.getWidth();
        if (w > bounds.getWidth())
            w = bounds.getWidth();
        
        auto h = bounds.getHeight();
        
        visibileRectangle->setRectangle(Rectangle<float>(x, y, w, h).reduced(1.f, 2.f));
        
    }

private:
    std::unique_ptr<juce::DrawableRectangle>    visibileRectangle;
    std::shared_ptr<AudiumEngine>               audiumEngine;
    std::shared_ptr<ZoomHandler>                zoomHandler;
    bool                                        arrangementMode;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DragZoomControl)
};
