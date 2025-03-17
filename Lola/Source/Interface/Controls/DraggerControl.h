//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
    DraggerControl(std::shared_ptr<audium::AudiumEngine> audiumEngine_,
                   std::shared_ptr<ZoomHandler> zoomHandler_,
                   juce::Colour colour_,
                   std::shared_ptr<RegionSelector> regionSelector_) :
        audiumEngine(audiumEngine_),
        zoomHandler(zoomHandler_),
        colour(colour_),
        regionSelector(regionSelector_)
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
    void mouseEnter (const MouseEvent& e) override;
    void mouseExit (const MouseEvent& e) override;
    
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
    
    void commitData(const juce::Range<double> newData, audium::TimeContextType context);
    
    bool commitPositionData(const audium::PositionableBase &positionableBase,
                            const juce::Range<double> newRange,
                            const audium::TimeContextType context);
    
    virtual bool isSelected() const = 0;
    
    virtual void setSelected(bool bSelected, bool deselectOthers) = 0;
    
    virtual void shiftSelect() = 0;
    
    virtual const juce::String getLabelString() const = 0;
    
    virtual bool validateData() = 0;
    
    void setComponentToDrag(juce::Component* comp);
    
    void setPositionableObject(std::shared_ptr<audium::PositionableBase> object);
    
    static constexpr int draggerHeight = 19;
    
    juce::Point<float> mouseDownOffset;
    
    juce::Point<int> autoScrollOffset;
    
protected:
    
    juce::Component* componentToDrag = nullptr;
    
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    juce::Colour colour;
    std::shared_ptr<RegionSelector> regionSelector;
    std::shared_ptr<audium::PositionableBase> positionableObject;
    
    Edge currentDragMode = outsideEdge;
    
    const int borderSize = 10;
    const int minimumWidth = 2;

    juce::Rectangle<int> originalBounds;

    std::unique_ptr<audium::UndoableContainerAction> undoableContainerAction;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DraggerControl)
};
