//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <memory>
#include <JuceHeader.h>
#include "Engine/PlayList/PlayListScheduler.h"

class SnapToGridHandler;

class ZoomHandler : private juce::Timer {
    
public:
    ZoomHandler(std::shared_ptr<audium::PlayListScheduler> playListScheduler,
                std::shared_ptr<SnapToGridHandler> snapToGridHandler);
    ~ZoomHandler() override;
    
    double zoomIn();
    double zoomOut();
        
    void setZoomFactor(double factor);
    double getZoomFactor() const noexcept;

    // the zoom factor a fresh project starts with
    static constexpr double defaultZoomFactor = 1.0;
    
    juce::Range<double> getVisibleRange() const noexcept;
    void setVisibleRange(juce::Range<double> newRange, juce::NotificationType notification = juce::sendNotificationAsync);
    
    juce::Range<double> getVisibleRangeInSeconds() const noexcept;
    
    void setViewport(juce::Viewport *view) { viewPort = view; }
    int getScrollBarHeight() const { return viewPort->getHorizontalScrollBar().getHeight(); }
    
    double secondsToXWithOffset (const double time) const;
    double xToSecondsWithOffset (const double x) const;
 
    double clocksToXWithOffset (const double clocks) const;
    double xToClocksWithOffset (const double x) const;

    juce::Range<double> secondsToX(juce::Range<double> seconds) const;
    juce::Range<double> xToSeconds(juce::Range<double> x) const;
    double secondsToX (const double seconds) const;
    double xToSeconds (const double x) const;
    
    double barsToX (const double bars) const;
    double xToBars (const double x) const;
    
    double beatsToX (const double beats) const;
    double xToBeats (const double x) const;
 
    // clocks = 96th (96 clocks = 1 bar)
    juce::Range<double> clocksToX(juce::Range<double> clocks) const;
    juce::Range<double> xToClocks(juce::Range<double> x) const;
    double clocksToX (const double clocks) const;

    /** Snaps a content x to the pixel it lands on ON SCREEN. The scrollbar
        holds the ideal scroll as a double while the viewport clamps it to an
        integer - quantizing only the content x combines two independent
        roundings and wobbles +/- 1 px against the scroll while zooming. */
    double snapContentX (const double x) const;
    double xToClocks (const double x) const;
    
    // returns a String in the format Min:Sec
    static juce::String secondsToFormattedString(const int seconds);

    bool snapToGrid(double &clocks);
    bool snapToGrid(juce::Range<double> &clocks);
    
    double reinterpretSeconds(const double seconds);
    
    void focusViewOnPlayPosition();
    
    void focusView(double positionInSeconds);
    
    void centerView(double positionInClocks, double center);
    
    void timerCallback() override;
    
    std::shared_ptr<audium::PlayListScheduler> getPlayListScheduler() const { return playListScheduler; }
    
    std::shared_ptr<SnapToGridHandler> getSnapToGridHandler() const { return snapToGridHandler; }
    
    double getContentWidth() const;
    
    void pageLeft();
    void pageRight();
    
    juce::Point<int> autoScroll(const juce::MouseEvent& e);
    
    juce::Viewport* getViewPort() const { return viewPort; }
 
    // max zoom in factor
    double maxZoomInFactor = 0.0;
    
    // max zoom out factor
    double maxZoomOutFactor = 0.0;
    
private:
        
    std::shared_ptr<audium::PlayListScheduler> playListScheduler;
    
    std::shared_ptr<SnapToGridHandler> snapToGridHandler;

    
    // the ViewPort of the arrangement view
    juce::Viewport* viewPort = nullptr;
    
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
