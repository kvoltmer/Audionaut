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
        
    void setZoomFactor(double factor);
    double getZoomFactor() const noexcept;
    
    juce::Range<double> getVisibleRange() const noexcept;
    void setVisibleRange(juce::Range<double> newRange, juce::NotificationType notification);
    
    juce::Range<double> getVisibleRangeInSeconds() const noexcept;
    
    void setHorizontalScrollBar(juce::ScrollBar* thescrollbar);
    
    double secondsToXWithOffset (const double time) const;
    double xToSecondsWithOffset (const double x) const;
 
    double clocksToXWithOffset (const double clocks) const;
    double xToClocksWithOffset (const double x) const;
    
    int getScrollBarHeight() const { return scrollbar->getHeight(); }

    juce::Range<double> secondsToX(juce::Range<double> seconds) const;
    juce::Range<double> xToSeconds(juce::Range<double> x) const;
    
    double secondsToX (const double seconds) const;
    double xToSeconds (const double x) const;
    
    double barsToX (const double bars) const;
    double xToBars (const double x) const;
    
    double beatsToX (const double beats) const;
    double xToBeats (const double x) const;
 
    double clocksToX (const double clocks) const;
    double xToClocks (const double x) const;


    
    // returns a String in the format Min:Sec
    static juce::String secondsToFormattedString(const int seconds);
    
    // returns the number of time segments for a given width. (1 second is the smallest possible grid)
    int numSegmentsForWidthInSeconds(const float width, int& seconds);
    
    // returns the number of time segments for a given width. (1 bar is the smallest possible grid)
    int numSegmentsForWidthInBars(const float width, int& bars);
    
    // returns the number of time segments for a given width. (1 beat is the smallest possible grid)
    int numSegmentsForWidthInBeats(const float width, int& beats);
    
    struct SegmentResult {
        int numSegments = 0;
        double itemWidth = 0.0;
        int grid = 0;
    };

    enum SegmentType {
        seconds = 0,
        bars = 1,
        beats = 2
    };
    
    const SegmentResult segmentsForWidth(const float totalWidth, SegmentType type);
    
    void focusViewOnPlayPosition();
    
    void focusView(double positionInSeconds);
    
    void centerView(double positionInClocks, double center);
    
    void timerCallback() override;
    
    std::shared_ptr<PlayListScheduler> getPlayListScheduler() const { return playListScheduler; }
    
    double getContentWidth() const;
    
    void pageLeft();
    void pageRight();
    
 
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
