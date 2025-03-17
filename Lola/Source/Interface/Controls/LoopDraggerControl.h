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

#include "Interface/ColourIds.h"

#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/AudiumEngine.h"

class LoopDraggerControl  : public juce::Component,
                            public juce::ChangeBroadcaster
{
public:
    LoopDraggerControl(juce::Component* componentToDrag_,
                       std::shared_ptr<AudiumEngine> audiumEngine_,
                       std::shared_ptr<ZoomHandler> zoomHandler_,
                       juce::Colour colour_,
                       std::shared_ptr<RegionSelector> regionSelector_) :
        componentToDrag(componentToDrag_),
        audiumEngine(audiumEngine_),
        zoomHandler(zoomHandler_),
        colour(colour_),
        regionSelector(regionSelector_)
    {
        if (componentToDrag == nullptr)
            componentToDrag = this;
    }

    ~LoopDraggerControl() override = default;
    


    void paint (juce::Graphics& g) override
    {
        if (paintMe) {
            auto colour = Colours::white;
            g.setColour (colour.withAlpha(0.8f));
            g.drawRect (getLocalBounds(), 1);   // draw an outline around the component
        }
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

        
    juce::Point<float> mouseDownOffset;
    
    juce::Point<int> autoScrollOffset;
    
    /** You can assign a lambda to this callback object to have it called when the value is changed. */
    std::function<void()> onValueChange;

    /** You can assign a lambda to this callback object to have it called when drag begins. */
    std::function<void()> onDragStart;

    /** You can assign a lambda to this callback object to have it called when drag ends. */
    std::function<void()> onDragEnd;
    
    juce::Range<double> loopRangeInClocks;
    
    bool paintMe = false;
    
protected:
    
    juce::Component* componentToDrag = nullptr;
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    juce::Colour colour;
    std::shared_ptr<RegionSelector> regionSelector;
    
    Edge currentDragMode = outsideEdge;
    
    const int borderSize = 10;
    const int minimumWidth = 2;

    juce::Rectangle<int> originalBounds;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopDraggerControl)
};
