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
#include "Interface/Widgets/audium_ListBox.h"

class ZoomHandler;
class AudiumEngine;
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
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    const int borderSize = 10;
    const int expandedWidth = 2;
    
    Point<int> dragStartPos;
    Point<int> dragEndPos;
    Point<int> moveStartPos;
    
    Edge currentDragMode = outsideEdge;
    
    bool avoidDragging = true;
    
    bool createRectangleAndSetBonds();
    
    
};
