/*
  ==============================================================================

    ZoomHandler.h
    Created: 17 Mar 2023 11:14:43am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <memory>
#include <JuceHeader.h>
#include "Engine/AudioResourceContainer.h"

using namespace juce;

class ZoomHandler {
    
public:
    ZoomHandler(std::shared_ptr<AudioResourceContainer> container);
    ~ZoomHandler();
    
    double zoomIn();
    
    double zoomOut();
    
    void setHorizontalScrollBar(juce::ScrollBar* thescrollbar);
    
    juce::ScrollBar* getHorizontalScrollBar() const { return scrollbar; }
    
    void updateTotalLength();
    
    int getWidth() const noexcept { return width; }
    
    void setWidth(int newWidth) { width = newWidth; }
    
    float timeToX (const double time) const;

    double xToTime (const float x) const;

private:
    
    // the audio resource container
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    
    // zoom factor
    double zoomFactor;
    
    // the scrollbar
    juce::ScrollBar* scrollbar;

    // the total range in seconds
    Range<double> totalRange;
    
    // the width in pixels
    int width;
    
};
