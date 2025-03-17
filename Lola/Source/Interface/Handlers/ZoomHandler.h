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
    
    juce::Range<double> getVisibleRange() const noexcept;
    void setVisibleRange(juce::Range<double> newRange, juce::NotificationType notification = juce::sendNotificationAsync);
    
    juce::Range<double> getVisibleRangeInSeconds() const noexcept;
    
    void setViewport(juce::Viewport *view) { viewPort = view; };
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
    
    bool snapToGrid(double &clocks);
    bool snapToGrid(juce::Range<double> &clocks);
    
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
