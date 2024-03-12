/*
  ==============================================================================

    GridView.h
    Created: 5 Mar 2024 10:59:47am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
*/
class GridView  : public juce::Component
{
public:
    GridView(std::shared_ptr<ZoomHandler> zoomHandler) :
        zoomHandler(zoomHandler)
    {
    }

    ~GridView() override
    {
    }

    void paint (juce::Graphics& g) override
    {
        paintTimeLineInBeats(g, getLocalBounds().toFloat());
        
        //std::cout << "GridView: " << getLocalBounds().getWidth() << " " << getLocalBounds().getHeight() << std::endl;
    }
    
    void paintTimeLineInBeats(juce::Graphics& g, Rectangle<float> rectangle)
    {
        g.setColour (juce::Colours::black.withAlpha(0.5f));
        auto segmentResult = zoomHandler->segmentsForWidth(rectangle.getWidth(), ZoomHandler::beats);
        auto x = rectangle.getX();
        for (auto i = 0; i < segmentResult.numSegments; i++)
        {
            g.fillRect(Rectangle<float>(x, rectangle.getY(), 1.f, rectangle.getHeight()));
            x += segmentResult.itemWidth;
        }
    }

    void resized() override
    {
        // This method is where you should set the bounds of any child
        // components that your component contains..


    }

private:
    
    std::shared_ptr<ZoomHandler> zoomHandler;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GridView)
};
