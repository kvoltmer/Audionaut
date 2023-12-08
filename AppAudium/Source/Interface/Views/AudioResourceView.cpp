/*
  ==============================================================================

    AudioResourceView.cpp
    Created: 27 Nov 2023 3:58:42pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "AudioResourceView.h"

#include "Engine/AudiumEngine.h"
#include "Engine/AudioRegionContainer.h"

#include "Interface/Controls/RegionEditControl.h"
#include "Interface/Controls/DraggerControl.h"

void AudioResourceView::paint (juce::Graphics& g)
{
    // std::cout << "AudioResourceView::paint" << std::endl;
    
    paintBackground(g);
    
    auto thumb = audioResource->getAudioThumbnail();
    jassert(thumb != nullptr);
    
    jassert(audioResource != nullptr);
    
    if (thumb->getTotalLength() > 0.0)
    {
        // the waveform colour
        g.setColour (colour);

        // calc the absolute x offset. (our top level component is 2 levels up)
        auto absoluteOffset = getLocalPoint (getParentComponent()->getParentComponent(), juce::Point<float> {0.f, 0.f}).getX();
        
        // the visible range is the scrollbar's range.
        auto visibleRange = zoomHandler->getVisibleRange();
        
        // adjust our visible range to local range
        visibleRange = visibleRange.movedToStartAt(visibleRange.getStart() + absoluteOffset);
        
        
        auto start = audioResource->getRegionDataInSeconds().getStart();
        start += zoomHandler->xToSeconds(visibleRange.getStart());
        
        // our local bounds
        auto thumbArea = getLocalBounds();
        thumbArea = thumbArea.withX(visibleRange.getStart());
        
        if (visibleRange.getLength() > thumbArea.getWidth())
        {
            thumbArea.setWidth(visibleRange.getLength());
        }
        
        auto end = start + zoomHandler->xToSeconds(thumbArea.getWidth());

        thumb->drawChannels (g, thumbArea, start, end, 1.0f);

        
//        std::cout << this << " DRAW x = " << thumbArea.getX() << " width = " << thumbArea.getWidth();
//        std::cout << " start = " << start << " length = " << end - start;
//        std::cout << std::endl;

        //paintFileNameLabel(g);
    }
}

void AudioResourceView::updateFromEngine()
{
    double posX = zoomHandler->secondsToX(audioResource->getTransportPosition());
    double length = zoomHandler->secondsToX(audioResource->getRegionDataInSeconds().getLength());
    
    // don't change Y position
    double posY = getBounds().getY();
    juce::Rectangle<double> rect_tmp(posX, posY, length, audioResource->getHeight());
    
    setBounds(rect_tmp.toNearestInt());
    
    
    if (mustRebuildComponents())
    {
        rebuildComponents();
        resized();
    }
    else
    {
        for (auto regionEdit : regionEditControls)
        {
            regionEdit->updateFromEngine();
        }
    }
    

}

void AudioResourceView::resized()
{
    auto regions = audiumEngine->getAudioRegionContainer()->getRegionsForGroup(audioResource->getAudioGroup());
    auto count = 0;
    for (auto region : regions)
    {
        if (count < regionEditControls.size())
        {
            auto regionEditControl = regionEditControls[count];
            regionEditControl->setBounds(0, DraggerControl::draggerHeight, 100, getHeight() - DraggerControl::draggerHeight);
            regionEditControl->updateFromEngine();
        }
        count++;
    }
}

bool AudioResourceView::mustRebuildComponents() const
{    
    auto regions = audiumEngine->getAudioRegionContainer()->getRegionsForGroup(audioResource->getAudioGroup());
    if (regions.size() != regionEditControls.size())
    {
        return true;
    }
    
    return false;
}

void AudioResourceView::rebuildComponents()
{
    std::cout << "AudioResourceView::rebuildComponents" << std::endl;
    
    regionEditControls.clear();
    
    auto regions = audiumEngine->getAudioRegionContainer()->getRegionsForGroup(audioResource->getAudioGroup());
    for (auto region : regions)
    {
        auto view = std::shared_ptr<RegionEditControl>(new RegionEditControl(region, zoomHandler, audiumEngine, regionSelector));
        addAndMakeVisible(view.get());
        regionEditControls.push_back(view);
    }
}
