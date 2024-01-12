/*
  ==============================================================================

    RegionEditControl.h
    Created: 29 Nov 2023 2:09:00pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "Interface/Widgets/audium_ListBox.h"

class ZoomHandler;
class AudiumEngine;
class AudioRegion;
class RegionSelector;

class RegionEditControl :   public juce::Component,
                            public juce::KeyListener
{
        
public:
    enum Edge
    {
        leftEdge,
        rightEdge,
        middleEdge,
        outsideEdge
    };
    
    RegionEditControl (std::shared_ptr<AudioRegion> audioRegion,
                       std::shared_ptr<ZoomHandler> zoomHandler,
                       std::shared_ptr<AudiumEngine> audiumEngine,
                       std::shared_ptr<RegionSelector> regionSelector) :
        audioRegion(audioRegion),
        zoomHandler(zoomHandler),
        audiumEngine(audiumEngine),
        regionSelector(regionSelector)
    {
        updateFromEngine();
        addKeyListener(this);
        setWantsKeyboardFocus(true);
    }
    
    ~RegionEditControl() override
    {
        removeKeyListener(this);
    }
    
    void paint (Graphics& g) override;
    
    void paintFileNameLabel (juce::Graphics& g);
    
    void mouseMove (const juce::MouseEvent& e) override;
    
    void mouseDown (const juce::MouseEvent& e) override;

    void mouseDrag (const juce::MouseEvent& e) override;

    void mouseUp (const juce::MouseEvent&) override;
    
    void updateMouseZone (const juce::MouseEvent& e);

    const Edge getDragMode(int x) const;
    
    void updateFromEngine();
    
    bool keyPressed (const KeyPress& key, Component* originatingComponent) override;

        
private:
    
    std::shared_ptr<AudioRegion> audioRegion;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<RegionSelector> regionSelector;
    
    const int borderSize = 10;
    const int minimumWidth = 2;
    
    Edge currentDragMode = outsideEdge;
    
    juce::Rectangle<int> originalBounds;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionEditControl)
    
};
