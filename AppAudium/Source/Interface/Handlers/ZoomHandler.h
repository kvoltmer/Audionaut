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

class AudioResourceContainer;
class TransportSourceProvider;

class ZoomHandler : private juce::Timer {
    
public:
    ZoomHandler(std::shared_ptr<AudioResourceContainer> container,
                std::shared_ptr<TransportSourceProvider> transportSourceProvider);
    ~ZoomHandler();
    
    double zoomIn();
    
    double zoomOut();
    
    juce::Range<double> getVisibleRange() const noexcept;
    
    juce::Range<double> getVisibleRangeInSeconds() const noexcept;
    
    juce::Range<double> getTotalRange() const noexcept;
    
    void setHorizontalScrollBar(juce::ScrollBar* thescrollbar);
    
    void updateTotalLength();
    
    int getWidth() const noexcept { return width; }
    
    void setWidth(int newWidth) { width = newWidth; }
    
    int timeToXWithOffset (const double time) const;
    
    double xToTimeWithOffset (const int x) const;
    
    int getScrollBarHeight() const { return scrollbar->getHeight(); }

    double timeToX (const double time) const;

    double xToTime (const double x) const;
    
    // returns a String in the format Min:Sec
    static juce::String secondsToFormattedString(const int seconds);
    
    // returns the number of time segments for a given width. (1 second is the smallest possible grid)
    int numSegmentsForWidth(const int width, int& seconds);
    
    void focusViewOnPlayPosition();
    
    void timerCallback() override;
    
private:
    
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    
    std::shared_ptr<TransportSourceProvider> transportSourceProvider;
    
    // zoom factor
    double zoomFactor;
    
    // the scrollbar
    juce::ScrollBar* scrollbar;

    // the total range in seconds
    juce::Range<double> totalRange;
    
    // the width in pixels
    int width;
    
};
