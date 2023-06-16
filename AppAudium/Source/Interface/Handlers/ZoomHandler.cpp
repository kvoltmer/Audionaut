/*
  ==============================================================================

    ZoomHandler.cpp
    Created: 17 Mar 2023 11:14:43am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "ZoomHandler.h"

ZoomHandler::ZoomHandler(std::shared_ptr<AudioResourceContainer> container) :
    audioResourceContainer(container),
    zoomFactor(1.0),
    scrollbar(nullptr),
    width(0)
{
}

ZoomHandler::~ZoomHandler()
{
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

Range<double> ZoomHandler::getVisibleRange() const noexcept
{
    jassert(scrollbar);
    return scrollbar->getCurrentRange();
}

Range<double> ZoomHandler::getVisibleRangeInSeconds() const noexcept
{
    // the visible range is the scrollbar's range
    auto visibleRange = getVisibleRange();
    
    // convert pixels to seconds (drawChannels expects start and end in seconds)
    return Range<double> (xToTime(visibleRange.getStart()), xToTime(visibleRange.getEnd()));
}

void ZoomHandler::setHorizontalScrollBar(juce::ScrollBar* thescrollbar)
{
    scrollbar = thescrollbar;
}



void ZoomHandler::updateTotalLength()
{
    Range<double> newRange (0.0, audioResourceContainer->getTotalLengthMax());
    
    totalRange = newRange;
}

double ZoomHandler::timeToX (const double time) const
{
    if (totalRange.getLength() <= 0)
        return 0;

    jassert(getWidth() > 0);
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

