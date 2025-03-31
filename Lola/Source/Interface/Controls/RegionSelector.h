//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <JuceHeader.h>
#include "Interface/Widgets/audium_ListBox.h"
#include "Engine/AudiumEngine.h"

class ZoomHandler;
class PlayListItemDraggerControl;
class SubGroupDraggerControl;

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
                    std::shared_ptr<audium::AudiumEngine> audiumEngine) :
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
        jassert(playListItemDraggerControls.size() == 0);
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
    
    // play list dragger controls
    std::vector<PlayListItemDraggerControl*> playListItemDraggerControls;
    
    // sub group dragger controls
    std::vector<SubGroupDraggerControl*> subGroupDraggerControls;
        
private:
    
    std::shared_ptr<audium::ListBox> owner;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    
    const int borderSize = 10;
    const int expandedWidth = 2;
    
    Point<int> dragStartPos;
    Point<int> dragEndPos;
    Point<int> moveStartPos;
    
    Edge currentDragMode = outsideEdge;
    
    bool avoidDragging = true;
    
    bool createRectangleAndSetBonds();
    
    
};
