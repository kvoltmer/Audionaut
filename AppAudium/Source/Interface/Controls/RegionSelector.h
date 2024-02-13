/*
  ==============================================================================

    RegionSelector.h
    Created: 25 May 2023 12:37:00pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "Interface/Widgets/audium_ListBox.h"

class ZoomHandler;
class AudiumEngine;

class RegionSelector :  public juce::Component,
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
    
    RegionSelector (std::shared_ptr<audium::ListBox> lb,
                    std::shared_ptr<ZoomHandler> zoomHandler,
                    std::shared_ptr<AudiumEngine> audiumEngine) :
        owner (lb),
        zoomHandler(zoomHandler),
        audiumEngine(audiumEngine)
    {
        owner->addMouseListener (this, true);
        addKeyListener(this);
        setWantsKeyboardFocus(true);
    }
    
    ~RegionSelector() override
    {
        owner->removeMouseListener (this);
        removeKeyListener(this);
    }
    
    void paint (Graphics& g) override;
    
    void mouseMove (const juce::MouseEvent& e) override;
    
    void mouseDown (const juce::MouseEvent& e) override;

    void mouseDrag (const juce::MouseEvent& e) override;

    void mouseUp (const juce::MouseEvent&) override;
    
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails&) override;
    
    bool keyPressed (const KeyPress& key,
                     Component* originatingComponent) override;
    
    void updateMouseZone (const juce::MouseEvent& e);

    const Edge getDragMode(int x) const;
    
    void updateFromEngine();
    
private:
    
    std::shared_ptr<audium::ListBox> owner;
    
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    const int borderSize = 10;
    const int expandedWidth = 2;
    
    Point<int> dragStartPos;
    Point<int> dragEndPos;
    Point<int> moveStartPos;
    
    Edge currentDragMode = outsideEdge;
    
    bool avoidDragging = true;
    
    void createRectangleAndSetBonds();
    
    
};
