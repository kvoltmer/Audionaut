//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Interface/ColourIds.h"

//==============================================================================
/*
*/
class TransportView  : public juce::Component
{
public:
    TransportView(std::shared_ptr<ZoomHandler> zoomHandler) :
        zoomHandler(zoomHandler)
    {
        setInterceptsMouseClicks(false, false);
    }

    ~TransportView() override
    {
    }

    void paintTimeLineInMinutesSeconds(juce::Graphics& g, juce::Rectangle<float> rectangle)
    {
        // draw the timeline in Min::Sec
        auto segmentResult = zoomHandler->segmentsForWidth(rectangle.getWidth(), ZoomHandler::seconds);
        auto x = rectangle.getX();
        auto seconds = 0;
        auto textColour = findColour(audium::defaultTextColourId);
        auto gridColour = juce::Colours::black;
        for (auto i = 0; i < segmentResult.numSegments; i++)
        {
            g.setColour (gridColour);
            g.fillRect(juce::Rectangle<float>(x, rectangle.getY(), 1.f, rectangle.getHeight()));
            
            // draw text
            g.setColour (textColour);
            g.setFont (12.0f);
            juce::Rectangle<float> bonds(x + 5.f, rectangle.getY(), segmentResult.itemWidth, rectangle.getHeight());
            g.drawText (ZoomHandler::secondsToFormattedString(seconds), bonds, juce::Justification::centredLeft, true);
            
            x += segmentResult.itemWidth;
            seconds += segmentResult.grid;
        }
    }
    
    void paintTimeLineInBars(juce::Graphics& g, juce::Rectangle<float> rectangle)
    {
        auto segmentResult = zoomHandler->segmentsForWidth(rectangle.getWidth(), ZoomHandler::bars);
        auto x = rectangle.getX();
        auto bars = 1;
        auto textColour = findColour(audium::defaultTextColourId);
        auto gridColour = juce::Colours::black;
        for (auto i = 0; i < segmentResult.numSegments; i++)
        {
            g.setColour (gridColour);
            g.fillRect(juce::Rectangle<float>(x, rectangle.getY(), 1.f, rectangle.getHeight()));
            
            // draw text
            g.setColour (textColour);
            g.setFont (12.0f);
            juce::Rectangle<float> bonds(x + 5.f, rectangle.getY(), segmentResult.itemWidth, rectangle.getHeight());
            g.drawText (String(bars), bonds, juce::Justification::centredLeft, true);
            
            x += segmentResult.itemWidth;
            bars += segmentResult.grid;
        }
    }
    
    void paint (juce::Graphics& g) override
    {
        
        auto bounds = getLocalBounds().toFloat();
        //std::cout << "TransportView: " << getLocalBounds().getWidth() << " " << getLocalBounds().getHeight() << std::endl;

        
        auto minSec = bounds.withBottom(bounds.getHeight() * 0.5f);
        paintTimeLineInMinutesSeconds(g, minSec);
        
        auto bars = bounds.withTop(bounds.getHeight() * 0.5f);
        paintTimeLineInBars(g, bars);
    }

    void resized() override
    {
        // This method is where you should set the bounds of any child
        // components that your component contains..

    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportView)
    
    std::shared_ptr<ZoomHandler> zoomHandler;
};
