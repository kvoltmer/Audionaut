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

class PlayListScheduler;

class ZoomHandler : private juce::Timer {
    
public:
    ZoomHandler(std::shared_ptr<PlayListScheduler> playListScheduler);
    ~ZoomHandler() override;
    
    double zoomIn();
    
    double zoomOut();
    
    juce::Range<double> getVisibleRange() const noexcept;
    
    juce::Range<double> getVisibleRangeInSeconds() const noexcept;
    
    void setHorizontalScrollBar(juce::ScrollBar* thescrollbar);
    
    double secondsToXWithOffset (const double time) const;
    double xToSecondsWithOffset (const double x) const;
    
    int getScrollBarHeight() const { return scrollbar->getHeight(); }

    double secondsToX (const double seconds) const;
    double xToSeconds (const double x) const;
    
    double barsToX (const double bars) const;
    double xToBars (const double x) const;
 
    double clocksToX (const double clocks) const;
    double xToClocks (const double x) const;

    
    // returns a String in the format Min:Sec
    static juce::String secondsToFormattedString(const int seconds);
    
    // returns the number of time segments for a given width. (1 second is the smallest possible grid)
    int numSegmentsForWidthInSeconds(const int width, int& seconds);
    
    // returns the number of time segments for a given width. (1 bar is the smallest possible grid)
    int numSegmentsForWidthInBars(const int width, int& beats);
    
    void focusViewOnPlayPosition();
    
    void focusView(double positionInSeconds);
    
    void timerCallback() override;
    
    std::shared_ptr<PlayListScheduler> getPlayListScheduler() const { return playListScheduler; }
    
    double getContentWidth() const;
    
    double getZoomFactor() const { return zoomFactor; }
    void setZoomFactor(double zoom) { zoomFactor = zoom; }
 
    // max zoom in factor
    double maxZoomInFactor = 0.0;
    
    // max zoom out factor
    double maxZoomOutFactor = 0.0;
    
private:
        
    std::shared_ptr<PlayListScheduler> playListScheduler;
    
    // the scrollbar
    juce::ScrollBar* scrollbar = nullptr;
    
    // zoom factor
    double zoomFactor = 0.0;
    

    
    // arrangement content width in pixels
    double arrangementContentWidth = 0.0;
        
    // define pixels per bar (4 beats or 96 clocks)
    double pixelsPerBar = 0.0;
    
    // the minimum arrangement length in bars
    double minimumArrangementBars = 30.0;
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZoomHandler)

};
