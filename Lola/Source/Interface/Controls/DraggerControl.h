/*
  ==============================================================================

    DraggerControl.h
    Created: 5 Dec 2023 11:53:47am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Handlers/SnapToGridHandler.h"
#include "Interface/Controls/RegionSelector.h"
#include "Engine/Resource/AudioResource.h"
#include "Interface/ColourIds.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/PlayList/PositionableBase.h"

class DraggerControl  : public juce::Component,
                        public juce::ChangeBroadcaster,
                        public juce::KeyListener
{
public:
    DraggerControl(juce::Component* componentToDrag,
                   std::shared_ptr<AudiumEngine> audiumEngine,
                   std::shared_ptr<ZoomHandler> zoomHandler,
                   juce::Colour colour,
                   std::shared_ptr<RegionSelector> regionSelector) :
        componentToDrag(componentToDrag),
        audiumEngine(audiumEngine),
        zoomHandler(zoomHandler),
        colour(colour),
        regionSelector(regionSelector)
    {
        addKeyListener(this);
        setWantsKeyboardFocus(true);
    }

    virtual ~DraggerControl() override
    {
        removeKeyListener(this);
    }
    
    void paintLabel (juce::Graphics& g, const juce::String label)
    {
        
        g.setFont (12.0f);
        
        juce::Rectangle<int> bonds(5,
                                   4,
                                   GlyphArrangement::getStringWidth (g.getCurrentFont(), label),
                                   g.getCurrentFont().getHeight());
        
        g.setColour (findColour(audium::defaultTextColourId));
        g.drawFittedText (label, bonds, juce::Justification::topLeft, 1);
    }

    void paint (juce::Graphics& g) override
    {
        auto colour = Colours::white;
        if (isSelected())
        {
            g.setColour (colour.withAlpha(0.8f));
        }
        else
        {
            g.setColour (colour.withAlpha(0.3f));
        }

        g.drawRect (getLocalBounds(), 1);   // draw an outline around the component

        paintLabel(g, getLabelString());

    }

    enum Edge
    {
        leftEdge,
        rightEdge,
        middleEdge,
        outsideEdge
    };
    
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    
    bool keyPressed (const KeyPress& key, Component* originatingComponent) override;

    void updateMouseZone (const juce::MouseEvent& e)
    {
        switch (getDragMode(e.getPosition().getX())) {
            case leftEdge:
                setMouseCursor (juce::MouseCursor::LeftEdgeResizeCursor);
                break;
            case rightEdge:
                setMouseCursor (juce::MouseCursor::RightEdgeResizeCursor);
                break;
            case middleEdge:
                setMouseCursor (juce::MouseCursor::DraggingHandCursor);
                break;
            default:
                break;
        }
    }

    const Edge getDragMode(int x) const
    {
        if (x < borderSize)
        {
            return leftEdge;
        }
        else if (getWidth() - x < borderSize)
        {
            return rightEdge;
        }
        else
        {
            return middleEdge;
        }
    }
    
    void commitRangeToEngine(juce::Range<double> rangeInClocks)
    {
        // commit values to engine
    
        commitData(rangeInClocks, audium::clocks);
    }
    
    virtual void commitData(const juce::Range<double> newData, audium::TimeContextType context) = 0;
    
    bool commitPositionData(const PositionableBase &positionableBase,
                            const juce::Range<double> newRange,
                            const audium::TimeContextType context);
    
    virtual bool isSelected() const = 0;
    
    virtual void setSelected(bool bSelected, bool deselectOthers) = 0;
    
    virtual const juce::String getLabelString() const = 0;
    
    virtual bool validateData() = 0;
    
    static constexpr int draggerHeight = 19;
    
    juce::Point<float> mouseDownOffset;
    
protected:
    
    juce::Component* componentToDrag;
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    juce::Colour colour;
    std::shared_ptr<RegionSelector> regionSelector;

    Edge currentDragMode = outsideEdge;
    
    const int borderSize = 10;
    const int minimumWidth = 2;

    juce::Rectangle<int> originalBounds;

    std::unique_ptr<audium::UndoableContainerAction> undoableContainerAction;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DraggerControl)
};
