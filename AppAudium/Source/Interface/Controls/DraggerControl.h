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
#include "Interface/Controls/RegionSelector.h"
#include "Engine/Resource/AudioResource.h"
#include "Interface/ColourIds.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"


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
                             g.getCurrentFont().getStringWidth(label),
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
    
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (regionSelector != nullptr)
            regionSelector->setEnabled(false);

        currentDragMode = getDragMode(e.getPosition().getX());
        
        originalBounds = componentToDrag->getBounds();
        
        setSelected(e.mods.isCommandDown() ? !isSelected() : true, !e.mods.isCommandDown());
        
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto distance = e.getOffsetFromDragStart();
        distance.setY(0); // drag vertically only
        if (std::abs(distance.getX()) > 0)
        {
            auto bounds = originalBounds;
            
            switch (currentDragMode) {
                case leftEdge:
                    bounds.setLeft(juce::jmin(originalBounds.getRight() - minimumWidth, originalBounds.getX() + distance.getX()));
                    break;
                case rightEdge:
                    bounds.setRight(juce::jmax(originalBounds.getX() + minimumWidth, originalBounds.getRight() + distance.getX()));
                    break;
                case middleEdge:
                    bounds += distance;
                    break;
                default:
                    jassertfalse;
                    break;
            }
            
            componentToDrag->setBounds (bounds);
            
            commitBoundsToEngine();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (regionSelector != nullptr)
            regionSelector->setEnabled(true);
        
        if (std::abs(e.getOffsetFromDragStart().getX()) > 0)
        {
            commitBoundsToEngine();
            validateData();
            sendChangeMessage();
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        updateMouseZone (e);
    }

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
    
    void commitBoundsToEngine()
    {
        // commit values to engine
        juce::Range<double> range(componentToDrag->getBounds().getX(), componentToDrag->getBounds().getRight());
        //std::cout << range.getStart() << " " << range.getEnd() << std::endl;
        
        auto rangeInSeconds = zoomHandler->xToSeconds(range);
        //std::cout << rangeInSeconds.getStart() << " " << rangeInSeconds.getEnd() << std::endl;
    
        setRegionDataInSeconds(rangeInSeconds);
    }
    
    virtual void setRegionDataInSeconds(const juce::Range<double> newRegionData) = 0;
    
    virtual bool isSelected() const = 0;
    
    virtual void setSelected(bool bSelected, bool deselectOthers) = 0;
    
    virtual const juce::String getLabelString() const = 0;
    
    virtual bool validateData() = 0;
    
    static constexpr int draggerHeight = 19;
    
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

    audium::UndoableContainerAction *undoableContainerAction = nullptr;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DraggerControl)
};
