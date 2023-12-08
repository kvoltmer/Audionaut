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
#include "Engine/AudioResource.h"
#include "Interface/ColourIds.h"
#include "Engine/AudioRegion.h"
#include "Engine/AudioGroup.h"

class DraggerControl  : public juce::Component, public juce::ChangeBroadcaster
{
public:
    DraggerControl(std::shared_ptr<AudioResource> audioResource,
                  std::shared_ptr<ZoomHandler> zoomHandler,
                  std::shared_ptr<AudioRegion> audioRegion,
                  juce::Colour colour,
                  std::shared_ptr<RegionSelector> regionSelector) :
        audioResource(audioResource),
        zoomHandler(zoomHandler),
        audioRegion(audioRegion),
        colour(colour),
        regionSelector(regionSelector)
    {
    }

    ~DraggerControl() override
    {
    }
    
    void paintFileNameLabel (juce::Graphics& g)
    {
        
        g.setFont (12.0f);
        
        juce::Rectangle<int> bonds(5,
                             4,
                             g.getCurrentFont().getStringWidth(audioResource->getFileNameWithoutExtension()),
                             g.getCurrentFont().getHeight());
        
        
        g.setColour (findColour(audium::defaultTextColourId));
        g.drawFittedText (audioResource->getFileNameWithoutExtension(), bonds, juce::Justification::topLeft, 1);
    }

    void paint (juce::Graphics& g) override
    {

        g.setColour (juce::Colour(juce::Colours::grey).withAlpha(0.5f));
        g.drawRect (getLocalBounds(), 1);   // draw an outline around the component

        paintFileNameLabel(g);

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
        
        originalBounds = getBounds();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto distance = e.getOffsetFromDragStart();
        distance.setY(0); // drag vertically only
        
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
        
        setBounds (bounds);
        
        commitBoundsToEngine();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (regionSelector != nullptr)
            regionSelector->setEnabled(true);
        
        if (audioResource->validateData())
        {
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
        juce::Range<double> range(getBounds().getX(), getBounds().getRight());
        //std::cout << range.getStart() << " " << range.getEnd() << std::endl;
        
        auto rangeInSeconds = zoomHandler->xToSeconds(range);
        //std::cout << rangeInSeconds.getStart() << " " << rangeInSeconds.getEnd() << std::endl;
    
        setRegionDataInSeconds(rangeInSeconds);
    }
    
    void setRegionDataInSeconds(const juce::Range<double> newRegionData)
    {
        // set value in the engine

        switch (currentDragMode) {
            case leftEdge:
                // offset in file
                {
                    auto diff = newRegionData.getStart() - audioResource->getTransportPosition();
                    
                    auto regionData = audioResource->getRegionDataInSeconds();
                    auto newLength = regionData.getLength() - diff;
                    auto newStart = regionData.getStart() + diff;
                
                    audioResource->setRegionDataInSeconds(juce::Range<double>(newStart, newStart + newLength));
                    audioResource->setTransportPosition(newRegionData.getStart());
                    repaint();
                }
                break;
            case rightEdge:
                {
                    // duration
                    auto regionData = audioResource->getRegionDataInSeconds();
                    regionData.setLength(newRegionData.getLength());
                    audioResource->setRegionDataInSeconds(regionData);
                }
                break;
            case middleEdge:
                // position in transport
                audioResource->setTransportPosition(newRegionData.getStart());
                break;
            default:
                break;
        }
        
        sendChangeMessage();
    }
    
    void updateFromEngine()
    {
        
        double posX = zoomHandler->secondsToX(audioResource->getTransportPosition());
        double length = zoomHandler->secondsToX(audioResource->getRegionDataInSeconds().getLength());
        
        // don't change Y position and height
        double posY = getBounds().getY();
        double height = getHeight();
        
        juce::Rectangle<double> rect_tmp(posX, posY, length, height);
        setBounds(rect_tmp.toNearestInt());
    }

    
private:
    
    std::shared_ptr<AudioResource> audioResource;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<AudioRegion> audioRegion;
    juce::Colour colour;
    std::shared_ptr<RegionSelector> regionSelector;

    Edge currentDragMode = outsideEdge;
    
    const int borderSize = 10;
    const int minimumWidth = 2;

    juce::Rectangle<int> originalBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DraggerControl)
};
