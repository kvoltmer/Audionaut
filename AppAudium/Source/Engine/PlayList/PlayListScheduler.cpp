/*
  ==============================================================================

    PlayListScheduler.cpp
    Created: 5 Jul 2023 3:22:44pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "PlayListScheduler.h"
#include "Engine/TransportSourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/ActionMessages.h"
#include "Engine/AudioGroupContainer.h"
#include "Engine/AudioGroup.h"
#include "Engine/AudioResource.h"
#include "Engine/AudiumTransportSource.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Link/LinkEngine.hpp"


using namespace::std::chrono;


void PlayListScheduler::prepareToPlay (double newSampleRate, int newBufferSize)
{
    sampleRate = newSampleRate;
    bufferSize = newBufferSize;

}

void PlayListScheduler::tick(bool isPlaying,
                             double beats,
                             int numSamples)
{
    if (isPlaying)
    {
        //auto beats = sessionState->beatAtTime(beginHostTime, quantum);
        if (beats >= 0.)
        {
            // assign absolute position
            transportPositionClocks = beatsToClocks(beats);
            
            // apply position in clocks
            applyAbsolutePosition(transportPositionClocks, numSamples);
        }
    }
}

void PlayListScheduler::audioCallback(const juce::AudioSourceChannelInfo& info)
{
    /// TODO: this is NOT thread save
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        auto group = audioGroupContainer->getAudioGroup(i);
        if (group != nullptr)
        {
            group->getTransportSourceContainer()->audioCallback(info);
        }
    }
}

double PlayListScheduler::absoluteToLocalPosition(double absolutePosition, const PlayListItem* item) const
{
    auto offset = absolutePosition - item->getAbsolueStartTime();
    return offset + item->getRegionData().getStart();
}

void PlayListScheduler::applyAbsolutePosition(double absolutePosition, int numSamples)
{
    // iterate over group container
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        const auto group = audioGroupContainer->getAudioGroup(i);
        const auto playlist = group->getPlayListContainer();
        const auto transport = group->getTransportSourceContainer();
        const auto clocksThisBuffer = secondsToClocks(static_cast<double>(numSamples) / sampleRate);
        const auto item = playlist->itemAtAbsolutePosition(absolutePosition + clocksThisBuffer);
        
        if (item != playlist->currentPlayListItem)
        {
            playlist->currentPlayListItem = item;
            
            if (item == nullptr)
            {
                transport->stopPlaying();
            }
            else
            {
                // set the position as accurate as possible
                const auto startPosition = playlist->currentPlayListItem->getAbsolueStartTime();
                const auto localPosition = absoluteToLocalPosition(startPosition, playlist->currentPlayListItem);
                
                const auto diff = startPosition - absolutePosition;
                const auto startSamples = static_cast<int>((clocksToSeconds(diff) * sampleRate) + 0.5);
                
                if (diff < 0.0)
                {
                    transport->setLocalPosition(clocksToSeconds(localPosition - diff), 0);
                }
                else
                {
                    jassert(startSamples < numSamples);
                    transport->setLocalPosition(clocksToSeconds(localPosition), startSamples);
                }
                
                std::cout << "pos " << absolutePosition << " start " <<  startPosition << " diff " << diff << " samples " << startSamples << std::endl;
                
                if (not transport->isPlaying())
                {
                    transport->startPlaying();
                }
            }
        }
    }
}

double PlayListScheduler::getTotalLengthClocks() const
{
    double totalLengthClocks = 0.0;
    
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        auto group = audioGroupContainer->getAudioGroup(i);
        totalLengthClocks = juce::jmax(totalLengthClocks, group->getPlayListContainer()->getTotalLength());
    }
    
    return totalLengthClocks;
}

double PlayListScheduler::getTotalLengthSeconds() const
{
    
    double totalLengthClocks = 0.0;
    
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        auto group = audioGroupContainer->getAudioGroup(i);
        totalLengthClocks = juce::jmax(totalLengthClocks, group->getPlayListContainer()->getTotalLength());
    }
    
    return clocksToSeconds(getTotalLengthClocks());
}


void PlayListScheduler::startPlaying()
{
    if (linkEngine != nullptr)
    {
        linkEngine->setStartPlayingTime(clocksToBeats(startPositionClocks));
        linkEngine->startPlaying();
    }
}

void PlayListScheduler::stopPlaying()
{
    resetCurrentPlayListItem();
    
    if (linkEngine != nullptr)
    {
        linkEngine->stopPlaying();
    }
    
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        auto group = audioGroupContainer->getAudioGroup(i);
        group->getTransportSourceContainer()->stopPlaying();
    }
}

bool PlayListScheduler::isPlaying() const
{
    if (linkEngine != nullptr)
    {
        return linkEngine->isPlaying();
    }
    jassertfalse;
    return false;
}

void PlayListScheduler::resetCurrentPlayListItem()
{
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
        audioGroupContainer->getAudioGroup(i)->getPlayListContainer()->currentPlayListItem = nullptr;
}

double PlayListScheduler::getAbsolutePositionClocks() const
{
    return isPlaying() ? transportPositionClocks : startPositionClocks;
}

double PlayListScheduler::getAbsolutePositionSeconds() const
{
    return clocksToSeconds(tempoProvider->getTempo(), isPlaying() ? transportPositionClocks : startPositionClocks);
}

void PlayListScheduler::setAbsolutePositionSeconds(double newPosition)
{
    if (linkEngine != nullptr)
    {
        startPositionClocks = secondsToClocks(tempoProvider->getTempo(), newPosition);
    }
}


int PlayListScheduler::getPlayListItemIndexAtCurrentPosition(std::shared_ptr<AudioGroup> group) const
{
    const auto item = group->getPlayListContainer()->itemAtAbsolutePosition(getAbsolutePositionClocks());
    if (item != nullptr)
    {
        return group->getPlayListContainer()->getPlayListItemIndex(item);
    }
    return -1;
}

void PlayListScheduler::setCurrentPositionAtPlayListItemIndex(std::shared_ptr<AudioGroup> group, int playListItemIndex)
{
    const auto item = group->getPlayListContainer()->getPlayListItem(playListItemIndex);
    if (item != nullptr)
    {
        startPositionClocks = item->getAbsolueStartTime();
    }
}

double PlayListScheduler::getPlayListItemProgress(std::shared_ptr<AudioGroup> group, int playListItemIndex) const
{
    const auto item = group->getPlayListContainer()->getPlayListItem(playListItemIndex);
    if (item != nullptr)
    {
        auto localPosition = absoluteToLocalPosition(getAbsolutePositionClocks(), item.get());
        auto progress = ((localPosition - item->getRegionData().getStart()) / item->getRegionData().getLength());
        return progress;
    }
    return 0.0;
}




