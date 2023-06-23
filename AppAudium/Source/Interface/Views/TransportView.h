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
        
                
        // calculate the with of time tile
        // get the width of one minute
        auto oneMinute = zoomHandler->timeToX(60);
        
        jassert(getWidth() > 0);
        auto itemWidth = oneMinute;
        auto numItems = (getWidth() / itemWidth) + 1;
        
        auto x = 0;
        auto seconds = 0.0;
        
        for (auto i = 0; i < numItems; i++)
        {
            // draw segment divider
            g.setColour (juce::Colours::black);
            g.drawLine(x, 0, x+1, getHeight());
            
            Rectangle<int> bonds(x + 5,
                                 0,
                                 itemWidth,
                                 getHeight());
            
            // draw text in Min:Sec format
            g.setColour (juce::Colours::white);
            g.setFont (12.0f);
            int min = (seconds > 0) ? (seconds / 60) : 0;
            int sec = seconds - (min * 60);
            String txt = juce::String::formatted("%d:%.2d\n", min, sec);
            g.drawFittedText (txt, bonds, juce::Justification::centredLeft, true);
            
            
            x += itemWidth;
            seconds += 60;
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
