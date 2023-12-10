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
#include "Engine/AudioResourceContainer.h"
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
    tempoProvider->prepareToPlay(newSampleRate, newBufferSize);
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
            if (loopPlayList.load())
            {
                transportPositionClocks = std::fmod(TempoProvider::beatsToClocks(beats), getTotalLengthClocks());
            }
            else
            {
                transportPositionClocks = TempoProvider::beatsToClocks(beats);
            }
            
            if (editMode)
            {
                processEditMode(transportPositionClocks, numSamples);
            }
            else
            {
                processArrangement(transportPositionClocks, numSamples);
            }
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
    return offset + item->getRegionDataInClocks().getStart();
}

void PlayListScheduler::processArrangement(double absolutePosition, int numSamples)
{
    // iterate over group container
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        const auto group = audioGroupContainer->getAudioGroup(i);
        const auto playlist = group->getPlayListContainer();
        const auto transport = group->getTransportSourceContainer();
        const auto clocksThisBuffer = getTempoProvider()->secondsToClocks(static_cast<double>(numSamples) / sampleRate);
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
                const auto startSamples = static_cast<int>(getTempoProvider()->clocksToSeconds(diff) * sampleRate);
                
                if (diff < 0.0)
                {
                    transport->setLocalPosition(getTempoProvider()->clocksToSeconds(localPosition - diff), 0);
                }
                else
                {
                    jassert(startSamples < numSamples);
                    transport->setLocalPosition(getTempoProvider()->clocksToSeconds(localPosition), startSamples);
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

void PlayListScheduler::processEditMode(double absolutePosition, int numSamples)
{
    // convert to seconds
    const auto positionInSeconds = tempoProvider->clocksToSeconds(absolutePosition);
    const auto secondsThisBuffer = static_cast<double>(numSamples) / sampleRate;

    const auto resources = audioResourceContainer->resourcesAtAbsolutePosition(positionInSeconds + secondsThisBuffer);
    for (auto resource : resources)
    {
        if (not resource->getAudioTransportSource()->isPlaying())
        {
            const auto startPosition = resource->getTransportPositionSeconds();
            const auto diff = startPosition - positionInSeconds;
            
            auto offset = startPosition - resource->getRegionDataInSeconds().getStart();
            //auto local = offset + item->getRegionDataInClocks().getStart();
            
            //const auto localPosition = absoluteToLocalPosition(startPosition, playlist->currentPlayListItem);
            
            //const auto localPosition = positionInSeconds + resource->getAbsolueStartTime();
            const auto localPosition = resource->getRegionDataInSeconds().getStart();
            
            /// TODO: calculate sample accurate position
            resource->getAudioTransportSource()->schedulePosition(localPosition, 0);
            resource->getAudioTransportSource()->start();
            resource->getAudioTransportSource()->startWithDuration(resource->getRegionDataInSeconds().getLength(), sampleRate);
        }
    }
}


double PlayListScheduler::getTotalLengthClocks() const
{
    if (isArrangementMode())
    {
        // in arrangement mode we calculate in clocks
        double totalLengthClocks = 0.0;
        
        for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
        {
            auto group = audioGroupContainer->getAudioGroup(i);
            totalLengthClocks = juce::jmax(totalLengthClocks, group->getPlayListContainer()->getTotalLength());
        }
        return totalLengthClocks;
    }
    else
    {
        return getTempoProvider()->secondsToClocks(getTotalLengthSeconds());
    }
}

double PlayListScheduler::getTotalLengthSeconds() const
{
    if (isArrangementMode())
    {
        return getTempoProvider()->clocksToSeconds(getTotalLengthClocks());
    }
    else
    {
        // in edit mode we calculate in seconds
        double totalLengthSeconds = 0.0;
        for (auto i = 0; i < audioResourceContainer->getNumAudioResources(); i++)
        {
            auto resource = audioResourceContainer->getAudioResource(i);
            totalLengthSeconds = std::max(totalLengthSeconds, resource->getAbsolueStartTime() + resource->getDurationTimeInSeconds());
        }
        return totalLengthSeconds;
    }
}


void PlayListScheduler::startPlaying()
{
    if (linkEngine != nullptr)
    {
        linkEngine->setStartPlayingTime(getTempoProvider()->clocksToBeats(startPositionClocks));
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
    return getTempoProvider()->clocksToSeconds(tempoProvider->getTempo(), isPlaying() ? transportPositionClocks : startPositionClocks);
}

void PlayListScheduler::setAbsolutePositionSeconds(double newPosition)
{
    if (linkEngine != nullptr)
    {
        startPositionClocks = getTempoProvider()->secondsToClocks(tempoProvider->getTempo(), newPosition);
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
        auto progress = ((localPosition - item->getRegionDataInClocks().getStart()) / item->getRegionDataInClocks().getLength());
        return progress;
    }
    return 0.0;
}

void PlayListScheduler::bounceToFile(juce::AudioFormatWriter* writer, double sampleRate, int numSamples, int numOutputChannels)
{
    auto lastPosition = getAbsolutePositionSeconds();
    setAbsolutePositionSeconds(0.0);
    startPlaying();
    
    auto seconds = getTotalLengthSeconds();
    auto iterations = static_cast<int>(seconds * sampleRate) / numSamples;
    iterations += 1; // add one iteration to be on the save side
    auto position = 0.0;
    juce::AudioBuffer<float> buffer(numOutputChannels, numSamples);
    juce::AudioSourceChannelInfo info (&buffer, 0, numSamples);
    
    for (auto i = 0; i < iterations; ++i)
    {
        const auto clocksThisBuffer = getTempoProvider()->secondsToClocks(static_cast<double>(numSamples) / sampleRate);
        const auto beatsThisBuffer = TempoProvider::clocksToBeats(clocksThisBuffer);
        
        tick(true, position, numSamples);
        position += beatsThisBuffer;
        
        
        buffer.clear();
        audioCallback(info);
        writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
        
        /// TODO: without waiting the output is fucked
        const auto waitMilliseconds = 2;
        juce::Time::waitForMillisecondCounter(juce::Time::getMillisecondCounter() + waitMilliseconds);
        
    }
    
    setAbsolutePositionSeconds(lastPosition);
    stopPlaying();
    
}



