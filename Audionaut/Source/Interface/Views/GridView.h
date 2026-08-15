//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Handlers/SnapToGridHandler.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

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
        // this component spans the whole content width (millions of pixels
        // when zoomed in), so segment generation is bounded by the dirty
        // region rather than the full bounds
        paintGridInBeats(g, getLocalBounds().toFloat(), g.getClipBounds().toFloat());
    }

    void paintGridInBeats(juce::Graphics& g, juce::Rectangle<float> drawingArea, juce::Rectangle<float> dirtyArea)
    {
        auto range = zoomHandler->clocksToX(currentRangeClocks);
        auto startInBeats = std::max(0.0, zoomHandler->xToBeats(dirtyArea.getX()));
        auto lengthInBeats = zoomHandler->xToBeats(dirtyArea.getWidth());
        auto snapToGridHandler = zoomHandler->getSnapToGridHandler();
        auto includePlayListItems = !range.isEmpty();
        auto segmentList = snapToGridHandler->getGridInSegments(dirtyArea.getWidth(),
                                                                lengthInBeats,
                                                                SnapToGridHandler::beats,
                                                                includePlayListItems,
                                                                startInBeats);

        for (auto seg : segmentList) {
            auto x = zoomHandler->beatsToX(seg.position);
            juce::Rectangle<float> gridLine(x, drawingArea.getY(), 1.f, drawingArea.getHeight());

            if (!range.isEmpty() &&
                !juce::ModifierKeys::currentModifiers.isShiftDown() &&
                (std::abs(static_cast<double>(x) - std::max(range.getStart(), 0.0)) < 10.0 ||
                 std::abs(static_cast<double>(x) - range.getEnd()) < 10.0)) {
                
                auto gridColour = findColour (audium::gridColourId);
                if (seg.isPlayListItem)
                    g.setColour (gridColour.withAlpha(1.f).brighter());
                else
                    g.setColour (gridColour.withAlpha(0.2f));
                
                g.setColour (seg.isPlayListItem ? gridColour.withAlpha(1.f).brighter() : gridColour);
                
                // draw dashed line
                juce::Line<float> line(gridLine.getTopLeft(), gridLine.getBottomLeft());
                const float myDashLength[] = { 6, 6 };
                g.drawDashedLine(line, &myDashLength[0], 2);
            }
            else {
                // draw regular grid line
                g.setColour (juce::Colours::black.withAlpha(0.5f));
                gridLine.setHeight(gridLine.getHeight() - AudiumLookAndFeel::extraSpaceAtBottom);
                g.fillRect(gridLine);
            }
        }
    }
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        auto snapToGridHandler = dynamic_cast<SnapToGridHandler*>(source);
        if (snapToGridHandler != nullptr)
        {
            currentRangeClocks = snapToGridHandler->getRange();
            repaint();
            //std::cout << "GridView " << currentRangeClocks.getStart() << " " << currentRangeClocks.getEnd() << std::endl;
        }
    }

private:
    
    std::shared_ptr<ZoomHandler> zoomHandler;
    
    juce::Range<double> currentRangeClocks;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GridView)
};
