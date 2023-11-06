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

#include "Engine/Link/LinkEngine.hpp"


using namespace::std::chrono;

PlayListScheduler::PlayListScheduler(std::shared_ptr<AudioGroupContainer> audioGroupContainer) :
    audioGroupContainer(audioGroupContainer)
{
}

PlayListScheduler::~PlayListScheduler()
{
}

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
                const auto startSamples = static_cast<int>(clocksToSeconds(diff) * sampleRate);
                
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
                
                if (isPlaying())
                {
                    if (not transport->isPlaying())
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
    // reset currentPlayListItem
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        auto group = audioGroupContainer->getAudioGroup(i);
        group->getPlayListContainer()->currentPlayListItem = nullptr;
    }
    
    if (linkEngine != nullptr)
    {
        linkEngine->setStartPlayingTime(clocksToBeats(startPositionClocks));
        linkEngine->startPlaying();
    }
}

void PlayListScheduler::stopPlaying()
{
    // reset currentPlayListItem
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        auto group = audioGroupContainer->getAudioGroup(i);
        group->getPlayListContainer()->currentPlayListItem = nullptr;
    }
    
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

double PlayListScheduler::getAbsolutePositionClocks() const
{
    return isPlaying() ? transportPositionClocks : startPositionClocks;
}

double PlayListScheduler::getAbsolutePositionSeconds() const
{
    return clocksToSeconds(tempoBPM, isPlaying() ? transportPositionClocks : startPositionClocks);
}

void PlayListScheduler::setAbsolutePositionSeconds(double newPosition)
{
    if (linkEngine != nullptr)
    {
        startPositionClocks = secondsToClocks(tempoBPM, newPosition);
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

void PlayListScheduler::onTriggerBeat(const double beatTime, const std::chrono::microseconds hostTime, int sampleNumber)
{
    typedef std::chrono::seconds seconds;

    seconds s = std::chrono::duration_cast<seconds>(hostTime);
    
    std::cout << "onTriggerBeat " << beatTime << " " << s.count() << " " << sampleNumber << std::endl;
}

void PlayListScheduler::setLinkEngine(audium::LinkEngine* engine)
{
    linkEngine = engine;
    linkEngine->mLink.setTempoCallback([this](const double p) { onTempoChange(p); });
    linkEngine->mLink.enable(false);
}

void PlayListScheduler::onTempoChange(double newTempo)
{
    tempoBPM = newTempo;
    sendActionMessage (tempoChanged);
}

double PlayListScheduler::getTempo() const
{
    return tempoBPM;
}


void PlayListScheduler::setTempo(double newTempo)
{
    tempoBPM = newTempo;
    if (linkEngine != nullptr)
    {
        linkEngine->setTempo(newTempo);
    }
    
    sendActionMessage (tempoChanged);
}


