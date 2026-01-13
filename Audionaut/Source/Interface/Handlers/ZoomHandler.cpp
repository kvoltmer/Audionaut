//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ZoomHandler.h"
#include "Interface/Handlers/SnapToGridHandler.h"

ZoomHandler::ZoomHandler(std::shared_ptr<audium::PlayListScheduler> playListScheduler_,
                         std::shared_ptr<SnapToGridHandler> snapToGridHandler_) :
    playListScheduler(playListScheduler_),
    snapToGridHandler(snapToGridHandler_)
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
    const auto arrangementBars = audium::TempoProvider::clocksToBars(clocks);
    
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
    if (viewPort != nullptr)
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

bool ZoomHandler::snapToGrid(double &clocks)
{
    return getSnapToGridHandler()->snapToGrid(*this, clocks);
}

bool ZoomHandler::snapToGrid(juce::Range<double> &clocks)
{
    return getSnapToGridHandler()->snapToGrid(*this, clocks);
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
