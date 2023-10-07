/*
  ==============================================================================

    ZoomHandler.cpp
    Created: 17 Mar 2023 11:14:43am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "ZoomHandler.h"
#include "Engine/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"

ZoomHandler::ZoomHandler(std::shared_ptr<AudioResourceContainer> container,
                         std::shared_ptr<PlayListScheduler> playListScheduler) :
    audioResourceContainer(container),
    playListScheduler(playListScheduler),
    zoomFactor(1.0),
    scrollbar(nullptr),
    width(0)
{
    startTimerHz(40);
}

ZoomHandler::~ZoomHandler()
{
    stopTimer();
}

double ZoomHandler::zoomIn()
{
    zoomFactor *= 2.0;
    return zoomFactor;
}

double ZoomHandler::zoomOut()
{
    zoomFactor /= 2.0;
    return zoomFactor;
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
    return juce::Range<double> (xToTime(visibleRange.getStart()), xToTime(visibleRange.getEnd()));
}

void ZoomHandler::setHorizontalScrollBar(juce::ScrollBar* thescrollbar)
{
    scrollbar = thescrollbar;
}



void ZoomHandler::updateTotalLength()
{
    juce::Range<double> newRange (0.0, audioResourceContainer->getTotalLengthMax());
    
    totalRange = newRange;
}

juce::Range<double> ZoomHandler::getTotalRange() const noexcept
{
    return totalRange;
}

double ZoomHandler::timeToX (const double time) const
{
    /// TODO: this is ugly
    if (getWidth() == 0)
    {
        return 0.0;
    }
    
    if (totalRange.getLength() <= 0)
        return 0;

    //jassert(getWidth() > 0);
    return (double) getWidth() * (double) ((time - totalRange.getStart()) / totalRange.getLength());
}

double ZoomHandler::xToTime (const double x) const
{
    jassert(getWidth() > 0);
    return (x / (double) getWidth()) * (totalRange.getLength()) + totalRange.getStart();
}

int ZoomHandler::timeToXWithOffset (const double time) const
{
    auto x = timeToX(time);
    auto offset = getVisibleRange().getStart();
    return juce::jmax (0.0, x - offset);
}

double ZoomHandler::xToTimeWithOffset (const int x) const
{
    auto offset = getVisibleRange().getStart();
    return juce::jmax (0.0, xToTime (static_cast<double>(x) + offset));
}

juce::String ZoomHandler::secondsToFormattedString(const int seconds)
{
    int min = (seconds > 0.0) ? (seconds / 60) : 0;
    int sec = seconds - (min * 60);
    return juce::String::formatted("%d:%.2d\n", min, sec);
}

int round2grid(int x)
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

int ZoomHandler::numSegmentsForWidth(const int width, int& seconds)
{
    jassert(width > 0);
    
    if (width > 0)
    {
        auto pixelsPerSec = width / xToTime(width);
        assert(pixelsPerSec > 0);
        // the duration for 100 pixels
        auto duration = int(100 / pixelsPerSec);
        // round to grid and assign the seconds parameter
        seconds = round2grid(duration);
        int itemWidth = timeToX(seconds);
        return (width / itemWidth) + 1;
    }
    
    return 0;
}

void ZoomHandler::focusViewOnPlayPosition()
{
    
    auto posX = timeToX(playListScheduler->getAbsolutePosition());
    auto range = getVisibleRange();
    //std::cout << pos << " " << posX << " range: " << range.getStart() << " " << range.getEnd() << std::endl;
    
    if (!range.contains(posX))
    {
        auto newStart = posX - (range.getLength() / 2);
        auto newRange = range.movedToStartAt(newStart);
        scrollbar->setCurrentRange(newRange);
        
    }
    
}

void ZoomHandler::timerCallback()
{
    if (playListScheduler->isPlaying())
    {
        auto posX = timeToX(playListScheduler->getAbsolutePosition());
        if(!getVisibleRange().contains(posX))
        {
            auto newStart = posX - (getVisibleRange().getLength() / 2);
            auto newRange = getVisibleRange().movedToStartAt(newStart);
            scrollbar->setCurrentRange(newRange);
        }
    }
}
