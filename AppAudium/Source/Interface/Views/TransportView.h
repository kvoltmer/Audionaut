/*
  ==============================================================================

    TransportView.h
    Created: 23 Jun 2023 4:07:39pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

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
        setInterceptsMouseClicks(false, true);
    }

    ~TransportView() override
    {
    }

    void paintTimeLineInMinutesSeconds(juce::Graphics& g, Rectangle<int> rectangle)
    {
        // draw the timeline in Min::Sec
        auto duration = 0;
        auto numSegments = zoomHandler->numSegmentsForWidthInSeconds(getWidth(), duration);
        if (numSegments > 0)
        {
            auto itemWidth = zoomHandler->timeToX(duration);
            auto x = rectangle.getX();
            auto seconds = 0;
            
            auto textColour = findColour(audium::defaultTextColourId);
            
            for (auto i = 0; i < numSegments; i++)
            {
                g.setColour (juce::Colours::black);
                
                // draw segment
                g.fillRect(Rectangle<int>(x, rectangle.getY(), 1, rectangle.getHeight()));
                
                // draw text in Min:Sec format
                g.setColour (textColour);
                g.setFont (12.0f);
                Rectangle<int> bonds(x + 5, rectangle.getY(), itemWidth, rectangle.getHeight());
                
                // draw text
                g.drawFittedText (ZoomHandler::secondsToFormattedString(seconds), bonds, juce::Justification::centredLeft, true);
                
                x += itemWidth;
                seconds += duration;
            }
        }
    }
    
    void paintTimeLineInBeats(juce::Graphics& g, Rectangle<int> rectangle)
    {
        // draw the timeline in beats
        auto duration = 0;
        auto numSegments = zoomHandler->numSegmentsForWidthInBars(getWidth(), duration);
        if (numSegments > 0)
        {
            auto itemWidth = zoomHandler->barsToX(duration);
            auto x = rectangle.getX();
            auto bars = 1;
            
            auto textColour = findColour(audium::defaultTextColourId);
            
            for (auto i = 0; i < numSegments; i++)
            {
                g.setColour (juce::Colours::black);
                
                // draw segment
                g.fillRect(Rectangle<int>(x, rectangle.getY(), 1, rectangle.getHeight()));
                
                // draw text in Min:Sec format
                g.setColour (textColour);
                g.setFont (12.0f);
                Rectangle<int> bonds(x + 5, rectangle.getY(), itemWidth, rectangle.getHeight());
                
                // draw text
                //g.drawFittedText (ZoomHandler::secondsToFormattedString(seconds), bonds, juce::Justification::centredLeft, true);
                g.drawFittedText (String(bars), bonds, juce::Justification::centredLeft, true);
                
                x += itemWidth;
                bars += duration;
            }
        }
    }
    
    void paint (juce::Graphics& g) override
    {
        // background
        g.fillAll(findColour(audium::backgroundColourId));
        
        Rectangle<int> minSec(0, 0, getWidth(), getHeight()/2);
        
        paintTimeLineInMinutesSeconds(g, minSec);
        
        Rectangle<int> beats(0, getHeight()/2, getWidth(), getHeight()/2);
        
        paintTimeLineInBeats(g, beats);
        
                

        
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
