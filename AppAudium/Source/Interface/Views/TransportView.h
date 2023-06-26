/*
  ==============================================================================

    TransportView.h
    Created: 23 Jun 2023 4:07:39pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

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

    void paint (juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::darkgrey);
        
                
        // draw the timeline in Min::Sec
        auto duration = 0;
        auto numSegments = zoomHandler->numSegmentsForWidth(getWidth(), duration);
        if (numSegments > 0)
        {
            auto itemWidth = zoomHandler->timeToX(duration);
            auto x = 0;
            auto seconds = 0;
            
            for (auto i = 0; i < numSegments; i++)
            {
                // draw segment divider
                g.setColour (juce::Colours::black);
                g.drawLine(x, 0, x + 1, getHeight());
                
                // draw text in Min:Sec format
                g.setColour (juce::Colours::white);
                g.setFont (12.0f);
                Rectangle<int> bonds(x + 5, 0, itemWidth, getHeight());
                g.drawFittedText (ZoomHandler::secondsToFormattedString(seconds), bonds, juce::Justification::centredLeft, true);
                
                x += itemWidth;
                seconds += duration;
            }
        }
        
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
