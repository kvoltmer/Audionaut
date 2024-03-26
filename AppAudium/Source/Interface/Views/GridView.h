/*
  ==============================================================================

    GridView.h
    Created: 5 Mar 2024 10:59:47am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Handlers/SnapToGridHandler.h"

//==============================================================================
/*
*/
class GridView  : public juce::Component, public juce::ChangeListener
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
        auto range = zoomHandler->clocksToX(currentRangeClocks);
        
        auto segmentResult = zoomHandler->segmentsForWidth(rectangle.getWidth(), ZoomHandler::beats);
        
        auto x = rectangle.getX();
        for (auto i = 0; i < segmentResult.numSegments; i++)
        {
            Rectangle<float> rect(x, rectangle.getY(), 1.f, rectangle.getHeight());
            
            if (!range.isEmpty() &&
                (std::abs(static_cast<double>(x) - std::max(range.getStart(), 0.0)) < 10.0 ||
                 std::abs(static_cast<double>(x) - range.getEnd()) < 10.0))
            {
                // draw dashed line
                g.setColour (juce::Colours::yellow.withAlpha(0.75f));
                juce::Line<float> line(rect.getTopLeft(), rect.getBottomLeft());
                const float myDashLength[] = { 6, 6 };
                g.drawDashedLine(line, &myDashLength[0], 2);
            }
            else
            {
                // draw regular grid line
                g.setColour (juce::Colours::black.withAlpha(0.5f));
                g.fillRect(rect);
            }
            
            x += segmentResult.itemWidth;
        }
    }
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        auto snapToGridHandler = dynamic_cast<SnapToGridHandler*>(source);
        if (snapToGridHandler != nullptr)
        {
            currentRangeClocks = snapToGridHandler->getRange();
            repaint();
            //std::cout << "GridView " << range.getStart() << " " << range.getEnd() << std::endl;
        }
    }

private:
    
    std::shared_ptr<ZoomHandler> zoomHandler;
    
    juce::Range<double> currentRangeClocks;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GridView)
};
