/*
  ==============================================================================

    ZoomHandler.cpp
    Created: 17 Mar 2023 11:14:43am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "ZoomHandler.h"
#include "Engine/PlayList/PlayListScheduler.h"


ZoomHandler::ZoomHandler(std::shared_ptr<PlayListScheduler> playListScheduler) :
    playListScheduler(playListScheduler),
    scrollbar(nullptr)
{
    // the default zoom factor
    zoomFactor = 1.0;
    
    // 1 bar = 200 pixels
    // 1 second = 100 pixels (@120BPM)
    pixelsPerBar = 200.0;
    
    // zoom in max = 10 times -> 1024
    maxZoomInFactor = std::pow(2.0, 10.0);
    
    // zoom out max = 10 times -> 0.0009765625
    maxZoomOutFactor = std::pow(0.5, 10);
        
    startTimerHz(40);
}

ZoomHandler::~ZoomHandler()
{
    stopTimer();
}

double ZoomHandler::zoomIn()
{
    zoomFactor *= 2.0;
    zoomFactor = std::min(zoomFactor, maxZoomInFactor);
    return zoomFactor;
}

double ZoomHandler::zoomOut()
{
    zoomFactor *= 0.5;
    zoomFactor = std::max(zoomFactor, maxZoomOutFactor);
    return zoomFactor;
}

void ZoomHandler::setZoomFactor(double factor)
{
    zoomFactor = factor;
    zoomFactor = std::min(zoomFactor, maxZoomInFactor);
    zoomFactor = std::max(zoomFactor, maxZoomOutFactor);
}

double ZoomHandler::getZoomFactor() const noexcept
{
    return zoomFactor;
}


double ZoomHandler::getContentWidth() const
{
    // get arrangement length from the playlist
    const auto clocks = playListScheduler->getTotalLength(audium::clocks);
    
    // add one bar to fit the content into arrangement
    const auto bars = TempoProvider::clocksToBars(clocks) + 1.0;
    
    // at least minimumArrangementBars
    const auto arrangementBars = std::max(minimumArrangementBars, bars);
    
    return arrangementBars * pixelsPerBar * zoomFactor;
}

juce::Range<double> ZoomHandler::getVisibleRange() const noexcept
{
    jassert(scrollbar);
    return scrollbar->getCurrentRange();
}

juce::Range<double> ZoomHandler::getVisibleRangeInSeconds() const noexcept
{
    // the visible range is the scrollbar's range
    auto visibleRange = getVisibleRange();
    
    // convert pixels to seconds (drawChannels expects start and end in seconds)
    return juce::Range<double> (xToSeconds(visibleRange.getStart()), xToSeconds(visibleRange.getEnd()));
}

void ZoomHandler::setHorizontalScrollBar(juce::ScrollBar* thescrollbar)
{
    scrollbar = thescrollbar;
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
    return std::max (0.0, x - offset);
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

int ZoomHandler::numSegmentsForWidthInSeconds(const int width, int& seconds)
{
    jassert(width > 0);
    
    if (width > 0)
    {
        auto pixelsPerSec = width / xToSeconds(width);
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

int ZoomHandler::numSegmentsForWidthInBars(const int width, int& bars)
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

void ZoomHandler::focusViewOnPlayPosition()
{
    auto posX = clocksToX(playListScheduler->getAbsolutePosition(audium::clocks));
    auto range = getVisibleRange();
    //std::cout << pos << " " << posX << " range: " << range.getStart() << " " << range.getEnd() << std::endl;
    
    if (!range.contains(posX))
    {
        auto newStart = posX - (range.getLength() / 2);
        auto newRange = range.movedToStartAt(newStart);
        scrollbar->setCurrentRange(newRange);
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
        scrollbar->setCurrentRange(newRange);
    }
}

void ZoomHandler::centerView(double positionInClocks, double center)
{
    const auto posX = clocksToX(positionInClocks);
    
    //std::cout << "seconds " << positionInSeconds << " " << posX << std::endl;
    const auto newStart = posX - (getVisibleRange().getLength() * center);
    const auto newRange = getVisibleRange().movedToStartAt(newStart);
    
    const auto oldRange = scrollbar->getCurrentRange();
    scrollbar->setCurrentRange(newRange);

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
            scrollbar->setCurrentRange(newRange);
        }
    }
}

void ZoomHandler::pageLeft()
{
    const auto newRange = getVisibleRange().movedToStartAt(getVisibleRange().getStart() - getVisibleRange().getLength());
    scrollbar->setCurrentRange(newRange);
}
void ZoomHandler::pageRight()
{
    const auto newRange = getVisibleRange().movedToStartAt(getVisibleRange().getEnd());
    scrollbar->setCurrentRange(newRange);
}
