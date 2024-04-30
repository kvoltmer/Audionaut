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
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/Resource/AudioResource.h"
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
            if (data.loopPlayList)
            {
                data.transportPositionClocks = std::fmod(TempoProvider::beatsToClocks(beats), getTotalLength(audium::clocks));
            }
            else
            {
                data.transportPositionClocks = TempoProvider::beatsToClocks(beats);
            }
            
            if (data.editMode)
            {
                processInEditMode(data.transportPositionClocks, numSamples);
            }
            else
            {
                processInArrangementMode(data.transportPositionClocks, numSamples);
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

double PlayListScheduler::absoluteToLocalPosition(double absolutePosition, const PlayListItem* item, audium::TimeContextType context) const
{
    auto offset = absolutePosition - item->getAbsolutePosition(context);
    return offset + item->getRegionData(audium::clocks).getStart();
}

void PlayListScheduler::processInArrangementMode(double absolutePosition, int numSamples)
{
    // iterate over group container
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        const auto group = audioGroupContainer->getAudioGroup(i);
        const auto playlist = group->getPlayListContainer();
        const auto transport = group->getTransportSourceContainer();
        const auto clocksThisBuffer = getTempoProvider()->secondsToClocks(static_cast<double>(numSamples) / sampleRate);
        const auto item = playlist->itemAtAbsolutePosition(absolutePosition + clocksThisBuffer, audium::clocks);
        
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
                const auto startPosition = playlist->currentPlayListItem->getAbsolutePosition(audium::clocks);
                const auto localPosition = absoluteToLocalPosition(startPosition, playlist->currentPlayListItem, audium::clocks);
                
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

void PlayListScheduler::processInEditMode(double absolutePosition, int numSamples)
{
    // convert to seconds
    const auto absolutePositionInSeconds = tempoProvider->clocksToSeconds(absolutePosition);
    const auto secondsThisBuffer = static_cast<double>(numSamples) / sampleRate;

    const auto resources = audioResourceContainer->resourcesAtAbsolutePosition(absolutePositionInSeconds + secondsThisBuffer);
    for (auto resource : resources)
    {
        if (not resource->getAudioTransportSource()->isPlaying())
        {
            const auto startPosition = resource->getAudioSubGroup()->getAudioClip()->getAbsolutePosition(audium::seconds);
            const auto localPosition = resource->getAudioSubGroup()->getAudioClip()->getRegionData(audium::seconds).getStart();
            const auto diff = startPosition - absolutePositionInSeconds;

            if (diff < 0.0)
            {
                resource->getAudioTransportSource()->schedulePosition(localPosition - diff, 0);
            }
            else
            {
                const auto startSamples = static_cast<int>(diff * sampleRate);
                jassert(startSamples < numSamples);
                resource->getAudioTransportSource()->schedulePosition(localPosition, startSamples);
            }
            
            std::cout << "absolute pos: " << absolutePositionInSeconds << " start " <<  startPosition << " diff " << diff << std::endl;

            resource->getAudioTransportSource()->scheduleDuration(resource->getAudioSubGroup()->getAudioClip()->getRegionData(audium::seconds).getLength(), sampleRate);
        }
    }
}


double PlayListScheduler::getTotalLength(audium::TimeContextType context, bool addOverhead) const
{
    double totalLength = 0.0;
    
    for (auto group : audioGroupContainer->getAudioGroups())
    {
        totalLength = juce::jmax(totalLength, group->getTotalLength(context, isArrangementMode()));
    }
        
    if (addOverhead)
    {
        // add overhead to fit entire arrangement arrangement
        auto overhead = 4 * 96.0;
        if (context == audium::seconds)
        {
            overhead = tempoProvider->clocksToSeconds(overhead);
        }
        
        overhead = std::max(overhead, totalLength * 0.01);
        
        
        totalLength += overhead;
    }
    
    return totalLength;
}

void PlayListScheduler::startPlaying()
{
    if (linkEngine != nullptr)
    {
        linkEngine->setStartPlayingTime(getTempoProvider()->clocksToBeats(data.startPositionClocks));
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

double PlayListScheduler::getAbsolutePosition(audium::TimeContextType context) const
{
    const auto clocks = isPlaying() ? data.transportPositionClocks : data.startPositionClocks;
    if (context == audium::clocks)
    {
        return clocks;
    }
    else if (context == audium::seconds)
    {
        return getTempoProvider()->clocksToSeconds(clocks);
    }
    
    jassertfalse;
    return 0.0;
}

void PlayListScheduler::setAbsolutePosition(double newPosition, audium::TimeContextType context)
{
    if (linkEngine != nullptr)
    {
        if (context == audium::clocks)
        {
            data.startPositionClocks = newPosition;
        }
        else if (context == audium::seconds)
        {
            data.startPositionClocks = getTempoProvider()->secondsToClocks(newPosition);
        }
    }
}


int PlayListScheduler::getPlayListItemIndexAtCurrentPosition(std::shared_ptr<AudioGroup> group) const
{
    const auto item = group->getPlayListContainer()->itemAtAbsolutePosition(getAbsolutePosition(audium::clocks), audium::clocks);
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
        data.startPositionClocks = item->getAbsolutePosition(audium::clocks);
    }
}

double PlayListScheduler::getPlayListItemProgress(std::shared_ptr<AudioGroup> group, int playListItemIndex) const
{
    const auto item = group->getPlayListContainer()->getPlayListItem(playListItemIndex);
    if (item != nullptr)
    {
        auto localPosition = absoluteToLocalPosition(getAbsolutePosition(audium::clocks), item.get(), audium::clocks);
        auto progress = ((localPosition - item->getRegionData(audium::clocks).getStart()) / item->getRegionData(audium::clocks).getLength());
        return progress;
    }
    return 0.0;
}

void PlayListScheduler::bounceToFile(juce::AudioFormatWriter* writer, double sampleRate, int numSamples, int numOutputChannels)
{
    // remember last position and reset to 0
    auto lastPosition = getAbsolutePosition(audium::seconds);
    setAbsolutePosition(0.0, audium::seconds);
    startPlaying();
    
    auto seconds = getTotalLength(audium::seconds);
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
    }
    
    setAbsolutePosition(lastPosition, audium::seconds);
    stopPlaying();
    
}



