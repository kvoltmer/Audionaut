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

#include "ZoomHandler.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Interface/Handlers/SnapToGridHandler.h"

ZoomHandler::ZoomHandler(std::shared_ptr<PlayListScheduler> playListScheduler,
                         std::shared_ptr<SnapToGridHandler> snapToGridHandler) :
    playListScheduler(playListScheduler),
    snapToGridHandler(snapToGridHandler)
{
    // the default zoom factor
    zoomFactor = 1.0;
    
    // 1 bar = 200 pixels
    // 1 second = 100 pixels (@120BPM)
    pixelsPerBar = 200.0;
    
    // zoom in max = 10 times -> 1024
    maxZoomInFactor = std::pow(2.0, 10.0);
    
    // zoom out max = 10 times -> 0.0009765625
    maxZoomOutFactor = std::pow(0.5, 10.0);
        
    startTimerHz(40);
}

ZoomHandler::~ZoomHandler()
{
    stopTimer();
}

double ZoomHandler::zoomIn()
{
    setZoomFactor(zoomFactor *= 2.0);
    return zoomFactor;
}

double ZoomHandler::zoomOut()
{
    setZoomFactor(zoomFactor *= 0.5);
    return zoomFactor;
}

void ZoomHandler::setZoomFactor(double factor)
{
    zoomFactor = std::min(factor, maxZoomInFactor);
    
    zoomFactor = std::max(factor, maxZoomOutFactor);
}

double ZoomHandler::getZoomFactor() const noexcept
{
    return zoomFactor;
}


double ZoomHandler::getContentWidth() const
{
    // get arrangement length from the playlist
    const auto clocks = playListScheduler->getTotalLength(audium::clocks, true);
    const auto arrangementBars = TempoProvider::clocksToBars(clocks);
    
    return arrangementBars * pixelsPerBar * zoomFactor;
}

juce::Range<double> ZoomHandler::getVisibleRange() const noexcept
{
    if (viewPort != nullptr) {
        return viewPort->getHorizontalScrollBar().getCurrentRange();
    }
    else {
        return juce::Range<double>();
    }
}

void ZoomHandler::setVisibleRange(juce::Range<double> newRange, juce::NotificationType notification)
{
    viewPort->getHorizontalScrollBar().setCurrentRange(newRange, notification);
}

juce::Range<double> ZoomHandler::getVisibleRangeInSeconds() const noexcept
{
    // the visible range is the scrollbar's range
    auto visibleRange = getVisibleRange();
    
    // convert pixels to seconds (drawChannels expects start and end in seconds)
    return juce::Range<double> (xToSeconds(visibleRange.getStart()), xToSeconds(visibleRange.getEnd()));
}


juce::Range<double> ZoomHandler::secondsToX(juce::Range<double> seconds) const
{
    return juce::Range<double>(secondsToX(seconds.getStart()), secondsToX(seconds.getEnd()));
}

juce::Range<double> ZoomHandler::xToSeconds(juce::Range<double> x) const
{
    return juce::Range<double>(xToSeconds(x.getStart()), xToSeconds(x.getEnd()));
}

double ZoomHandler::secondsToX (const double seconds) const
{
    const auto beats = playListScheduler->getTempoProvider()->secondsToBeats(seconds);
    const auto bars = beats * 0.25;
    return barsToX(bars);
}

double ZoomHandler::xToSeconds (const double x) const
{
    const auto bars = xToBars(x);
    const auto beats = bars * 4.0;
    return playListScheduler->getTempoProvider()->beatsToSeconds(beats);
}

double ZoomHandler::barsToX (const double bars) const
{
    return (bars * pixelsPerBar) * zoomFactor;
}

double ZoomHandler::xToBars (const double x) const
{
    return x * (1.0 / pixelsPerBar) / zoomFactor;
}

double ZoomHandler::beatsToX (const double beats) const
{
    return barsToX(beats * 0.25);
}

double ZoomHandler::xToBeats (const double x) const
{
    return xToBars(x) * 4.0;
}

juce::Range<double> ZoomHandler::clocksToX(juce::Range<double> clocks) const
{
    return juce::Range<double>(clocksToX(clocks.getStart()), clocksToX(clocks.getEnd()));
}

juce::Range<double> ZoomHandler::xToClocks(juce::Range<double> x) const
{
    return juce::Range<double>(xToClocks(x.getStart()), xToClocks(x.getEnd()));
}

double ZoomHandler::clocksToX (const double clocks) const
{
    return barsToX(clocks / 96.0);
}

double ZoomHandler::xToClocks (const double x) const
{
    return xToBars(x) * 96.0;
}

double ZoomHandler::secondsToXWithOffset (const double time) const
{
    auto x = secondsToX(time);
    auto offset = getVisibleRange().getStart();
    return x - offset;
}

double ZoomHandler::xToSecondsWithOffset (const double x) const
{
    auto offset = getVisibleRange().getStart();
    return std::max (0.0, xToSeconds (x + offset));
}

double ZoomHandler::clocksToXWithOffset (const double clocks) const
{
    auto x = clocksToX(clocks);
    auto offset = getVisibleRange().getStart();
    return std::max (0.0, x - offset);
}

double ZoomHandler::xToClocksWithOffset (const double x) const
{
    auto offset = getVisibleRange().getStart();
    return std::max (0.0, xToClocks (x + offset));
}


juce::String ZoomHandler::secondsToFormattedString(const int seconds)
{
    int min = (seconds > 0.0) ? (seconds / 60) : 0;
    int sec = seconds - (min * 60);
    return juce::String::formatted("%d:%.2d\n", min, sec);
}

int roundSecondsToGrid(int x)
{
    if (x < 1)
    {
        return 1;
    }
    else if (x < 5)
    {
        return 5;
    }
    else if (x < 15)
    {
        return 15;
    }
    else
    {
        return x + 30 - x % 30;
    }
}

int ZoomHandler::numSegmentsForWidthInSeconds(const float width, int& seconds)
{
    jassert(width > 0);
    
    if (width > 0)
    {
        auto pixelsPerSec = width / static_cast<float>(xToSeconds(width));
        assert(pixelsPerSec > 0);
        // the duration for 100 pixels
        auto duration = int(100 / pixelsPerSec);
        // round to grid and assign the seconds parameter
        seconds = roundSecondsToGrid(duration);
        int itemWidth = secondsToX(seconds);
        return (width / itemWidth) + 1;
    }
    
    return 0;
}

int roundBarsToGrid(int x)
{
    if (x < 1)
    {
        return 1;
    }
    else if (x < 4)
    {
        return 4;
    }
    else if (x < 16)
    {
        return 16;
    }
    else
    {
        return x + 32 - x % 32;
    }
}

int ZoomHandler::numSegmentsForWidthInBars(const float width, int& bars)
{
    jassert(width > 0);
    
    if (width > 0)
    {
        auto pixelsPerBar = width / xToBars(width);
        assert(pixelsPerBar > 0);
        // the duration for 50 pixels
        auto duration = int(50 / pixelsPerBar);
        // round to grid and assign the seconds parameter
        bars = roundBarsToGrid(duration);
        int itemWidth = barsToX(bars);
        return (width / itemWidth) + 1;
    }
    
    return 0;
}

int roundBeatsToGrid(int x)
{
    if (x < 1)
    {
        return 1;
    }
    else if (x < 4)
    {
        return 4;
    }
    else if (x < 16)
    {
        return 16;
    }
    else
    {
        return x + 32 - x % 32;
    }
}

int ZoomHandler::numSegmentsForWidthInBeats(const float width, int& beats)
{
    jassert(width > 0);
    
    if (width > 0)
    {
        auto pixelsPerBeat = width / (xToBeats(width));
        assert(pixelsPerBeat > 0);
        // the duration in pixels
        auto duration = int(25.0 / pixelsPerBeat);
        // round to grid 
        beats = roundBeatsToGrid(duration);
        int itemWidth = beatsToX(beats);
        return (width / itemWidth) + 1;
    }
    
    return 0;
}

const ZoomHandler::SegmentResult ZoomHandler::segmentsForWidth(const float totalWidth, ZoomHandler::SegmentType type)
{
    jassert(totalWidth > 0);
    SegmentResult result;
   
    double pixelsPerSegment = 0.0;
    int duration = 0;
    
    switch (type) {
        case seconds:
            pixelsPerSegment = totalWidth / static_cast<float>(xToSeconds(totalWidth));
            duration = int(100.0 / pixelsPerSegment);
            result.grid = roundSecondsToGrid(duration);
            result.itemWidth = secondsToX(result.grid);
            break;
        case beats:
            pixelsPerSegment = totalWidth / static_cast<float>(xToBeats(totalWidth));
            duration = int(25.0 / pixelsPerSegment);
            result.grid = roundBeatsToGrid(duration);
            result.itemWidth = beatsToX(result.grid);
            break;
        case bars:
            pixelsPerSegment = totalWidth / static_cast<float>(xToBars(totalWidth));
            duration = int(50.0 / pixelsPerSegment);
            result.grid = roundBarsToGrid(duration);
            result.itemWidth = barsToX(result.grid);
            break;
        default:
            break;
    }
    
    result.numSegments = (totalWidth / result.itemWidth) + 1;
    return result;
}

bool ZoomHandler::snapToGrid(double &clocks)
{
    if (!juce::ModifierKeys::currentModifiers.isShiftDown()) {
        
        auto segmentResult  = segmentsForWidth(getContentWidth(), ZoomHandler::beats);
        auto beats          = TempoProvider::clocksToBeats(clocks);
        auto tolerance      = xToBeats(10.0);
        auto inc            = 0.0;
        
        for (auto i = 0; i < segmentResult.numSegments; i++) {
            if (std::abs(inc - std::max(beats, 0.0)) < tolerance) {
                //std::cout << "snap to: " << inc << std::endl;
                clocks = TempoProvider::beatsToClocks(inc);
                return true;
            }
            inc += segmentResult.grid;
        }
    }
    
    return false;
}

bool ZoomHandler::snapToGrid(juce::Range<double> &clocks)
{
    double start = clocks.getStart();
    double end = clocks.getEnd();
    
    bool snapStart = snapToGrid(start);
    bool snapEnd = snapToGrid(end);
        
    if (snapStart && snapEnd) {
        clocks = juce::Range<double>(start, end);
    }
    else if (snapStart && !snapEnd) {
        clocks = clocks.withStart(start);
    }
    else if (!snapStart && snapEnd) {
        clocks = clocks.withEnd(end);
    }
 
    return (snapStart || snapEnd);
}


void ZoomHandler::focusViewOnPlayPosition()
{
    auto posX = clocksToX(playListScheduler->getAbsolutePosition(audium::clocks));
    auto range = getVisibleRange();
    //std::cout << pos << " " << posX << " range: " << range.getStart() << " " << range.getEnd() << std::endl;
    
    if (!range.contains(posX))
    {
        auto newStart = posX - (range.getLength() / 2);
        auto newRange = range.movedToStartAt(newStart);
        setVisibleRange(newRange);
    }
}

void ZoomHandler::focusView(double positionInSeconds)
{
    const auto posX = secondsToX(positionInSeconds);
    
    if(!getVisibleRange().contains(posX))
    {
        //std::cout << "seconds " << positionInSeconds << " " << posX << std::endl;
        const auto newStart = posX - (getVisibleRange().getLength() * 0.5);
        const auto newRange = getVisibleRange().movedToStartAt(newStart);
        setVisibleRange(newRange);
    }
}

void ZoomHandler::centerView(double positionInClocks, double center)
{
    const auto posX = clocksToX(positionInClocks);
    
    //std::cout << "seconds " << positionInSeconds << " " << posX << std::endl;
    const auto newStart = posX - (getVisibleRange().getLength() * center);
    const auto newRange = getVisibleRange().movedToStartAt(newStart);
    
    setVisibleRange(newRange);

}

void ZoomHandler::timerCallback()
{
    if (playListScheduler->isPlaying() &&
        playListScheduler->getFollowTransport())
    {
        
        auto posX = secondsToX(playListScheduler->getAbsolutePosition(audium::seconds));
        if(!getVisibleRange().contains(posX))
        {
            //std::cout << "seconds " << playListScheduler->getAbsolutePositionSeconds() << " " << posX << std::endl;
            auto newStart = posX;// - (getVisibleRange().getLength() / 2);
            auto newRange = getVisibleRange().movedToStartAt(newStart);
            setVisibleRange(newRange);
        }
    }
}

void ZoomHandler::pageLeft()
{
    const auto newRange = getVisibleRange().movedToStartAt(getVisibleRange().getStart() - getVisibleRange().getLength());
    setVisibleRange(newRange);
}
void ZoomHandler::pageRight()
{
    const auto newRange = getVisibleRange().movedToStartAt(getVisibleRange().getEnd());
    setVisibleRange(newRange);
}

juce::Point<int> ZoomHandler::autoScroll(const juce::MouseEvent& e)
{
    auto pos = e.getEventRelativeTo(getViewPort()).getPosition();
    // std::cout << "autoScroll pos " << pos.getX() << std::endl;
    auto oldPoint = getViewPort()->getViewedComponent()->getBounds().getTopLeft();
    if (getViewPort()->autoScroll(pos.getX(), pos.getY(), 30, 20)) {
        auto newPoint = getViewPort()->getViewedComponent()->getBounds().getTopLeft();
        return oldPoint - newPoint;
    }
    return juce::Point<int>(0, 0);
}
