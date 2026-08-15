//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Interface/ColourIds.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

//==============================================================================
/*
*/
class TransportView  : public juce::Component
{
public:
    TransportView(std::shared_ptr<ZoomHandler> zoomHandler_) :
        zoomHandler(zoomHandler_)
    {
        setInterceptsMouseClicks(false, false);
    }

    ~TransportView() override
    {
    }

    void paintTimeLineInMinutesSeconds(juce::Graphics& g, juce::Rectangle<float> rectangle, juce::Rectangle<float> dirtyArea)
    {
        // draw the timeline in Min::Sec
        auto startInSeconds = std::max(0.0, zoomHandler->xToSeconds(dirtyArea.getX()));
        auto lengthInSeconds = zoomHandler->xToSeconds(dirtyArea.getWidth());
        auto snapToGridHandler = zoomHandler->getSnapToGridHandler();
        auto segmentList = snapToGridHandler->getGridInSegments(dirtyArea.getWidth(),
                                                                lengthInSeconds,
                                                                SnapToGridHandler::seconds,
                                                                false,
                                                                startInSeconds);
        
        auto textColour = findColour(audium::defaultTextColourId);
        auto gridColour = juce::Colours::black;
        
        for (auto seg : segmentList) {
            
            auto seconds = seg.position;
            auto x = zoomHandler->secondsToX(seconds);
            auto itemWidth = zoomHandler->secondsToX(seg.grid);
            g.setColour (gridColour);
            g.fillRect(juce::Rectangle<float>(x,
                                              rectangle.getY(),
                                              1.f,
                                              rectangle.getHeight()));
            
            // draw text Min::Sec
            g.setColour (textColour);
            g.setFont (AudiumLookAndFeel::withTabularFigures (juce::FontOptions (12.0f)));
            juce::Rectangle<float> bonds(x + 5.f,
                                         rectangle.getY(),
                                         itemWidth,
                                         rectangle.getHeight());
            g.drawText (ZoomHandler::secondsToFormattedString(seconds),
                        bonds,
                        juce::Justification::centredLeft,
                        true);

        }
    }
    
    void paintTimeLineInBars(juce::Graphics& g, juce::Rectangle<float> rectangle, juce::Rectangle<float> dirtyArea)
    {
        auto startInBars = std::max(0.0, zoomHandler->xToBars(dirtyArea.getX()));
        auto lengthInBars = zoomHandler->xToBars(dirtyArea.getWidth());
        auto snapToGridHandler = zoomHandler->getSnapToGridHandler();
        auto segmentList = snapToGridHandler->getGridInSegments(dirtyArea.getWidth(),
                                                                lengthInBars,
                                                                SnapToGridHandler::bars,
                                                                false,
                                                                startInBars);
        
        auto textColour = findColour(audium::defaultTextColourId);
        auto gridColour = juce::Colours::black;
        
        for (auto seg : segmentList) {
            
            auto bars = seg.position;
            auto x = zoomHandler->barsToX(bars);
            auto itemWidth = zoomHandler->barsToX(seg.grid);

            g.setColour (gridColour);
            g.fillRect(juce::Rectangle<float>(x, rectangle.getY(), 1.f, rectangle.getHeight()));
            
            // draw text
            g.setColour (textColour);
            g.setFont (AudiumLookAndFeel::withTabularFigures (juce::FontOptions (12.0f)));
            juce::Rectangle<float> bonds(x + 5.f, rectangle.getY(), itemWidth, rectangle.getHeight());
            g.drawText (String(bars + 1), bonds, juce::Justification::centredLeft, true);
        }
    }
    
    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // spans the whole content width - bound the segment generation by the
        // dirty region (the header's 60 Hz markers dirty narrow stripes)
        auto dirtyArea = g.getClipBounds().toFloat();

        auto minSec = bounds.withBottom(bounds.getHeight() * 0.5f);
        paintTimeLineInMinutesSeconds(g, minSec, dirtyArea);

        auto bars = bounds.withTop(bounds.getHeight() * 0.5f);
        paintTimeLineInBars(g, bars, dirtyArea);
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
