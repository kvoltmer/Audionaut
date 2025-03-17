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
#include "Engine/AudiumEngine.h"
#include "Engine/Region/AudioRegion.h"

class ZoomHandler;
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
    
    RegionEditControl (std::shared_ptr<audium::AudioRegion> audioRegion,
                       std::shared_ptr<ZoomHandler> zoomHandler,
                       std::shared_ptr<audium::AudiumEngine> audiumEngine,
                       std::shared_ptr<RegionSelector> regionSelector) :
        audioRegion(audioRegion),
        zoomHandler(zoomHandler),
        audiumEngine(audiumEngine),
        regionSelector(regionSelector)
    {
        updateFromEngine(audioRegion);
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
    
    void updateFromEngine(std::shared_ptr<audium::AudioRegion> audioRegion);
    
    bool keyPressed (const KeyPress& key, Component* originatingComponent) override;

        
private:
    
    std::shared_ptr<audium::AudioRegion> audioRegion;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<RegionSelector> regionSelector;
    
    const int borderSize = 10;
    const int minimumWidth = 2;
    
    Edge currentDragMode = outsideEdge;
    
    juce::Rectangle<int> originalBounds;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionEditControl)
    
};
