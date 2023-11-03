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

void PlayListScheduler::tick(double beats,
                             int numSamples)
{
    if (isPlaying())
    {
        //auto beats = sessionState->beatAtTime(beginHostTime, quantum);
        if (beats >= 0.)
        {
            // assign absolute position
            transportPositionClocks = beatsToClocks(beats);
            
            // for now we use a position in seconds
            //applyAbsolutePosition(clocksToSeconds(tempoBPM, transportPositionClocks));
            
            applyAbsolutePosition(transportPositionClocks);
        }
    }
}

double PlayListScheduler::absoluteToLocalPosition(double absolutePosition, const PlayListItem* item) const
{
    auto offset = absolutePosition - item->getAbsolueStartTime();
    return offset + item->getRegionData().getStart();
}

void PlayListScheduler::applyAbsolutePosition(double pos)
{
    // lookup current play list item
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        auto group = audioGroupContainer->getAudioGroup(i);
        auto item = group->getPlayListContainer()->itemAtAbsolutePosition(pos);

        // assign current item and stop
        if (item != group->getPlayListContainer()->currentPlayListItem)
        {
            group->getPlayListContainer()->currentPlayListItem = item;
            group->getTransportSourceContainer()->stopPlaying();
        }

        // apply position and start if needed
        if (group->getPlayListContainer()->currentPlayListItem != nullptr)
        {
            if (not group->getTransportSourceContainer()->isPlaying() || forcePosition.load())
            {
                forcePosition.store(false);
                const auto clocks = absoluteToLocalPosition(pos, group->getPlayListContainer()->currentPlayListItem);
                group->getTransportSourceContainer()->setLocalPosition(clocksToSeconds(clocks));
                if (isPlaying())
                {
                    group->getTransportSourceContainer()->startPlaying();
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

int PlayListScheduler::getPlayListItemIndex(std::shared_ptr<AudioGroup> group) const
{

    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        auto g = audioGroupContainer->getAudioGroup(i);
        if (g == group)
        {
            return g->getPlayListContainer()->getPlayListItemIndex(g->getPlayListContainer()->currentPlayListItem);
        }
        
    }
    return 0;
}

void PlayListScheduler::setPlayListItemIndex(int playListItemIndex)
{
    // TODO: implement
//    auto items = playListContainer->getPlayListItems();
//    if (playListItemIndex < items.size())
//    {
//        auto item = items[playListItemIndex];
//
//        auto itemStartPosition = item->getAbsolueStartTime();
//
//        setAbsolutePosition(itemStartPosition);
//
//        if (not isPlaying())
//        {
//            start();
//        }
//    }
//    else
//    {
//        jassertfalse;
//    }
}

double PlayListScheduler::getPlayListItemProgress(std::shared_ptr<AudioGroup> group, int playListItemIndex) const
{
    // TODO: implement
    auto currentPlayListItem = group->getPlayListContainer()->currentPlayListItem;
    if (playListItemIndex == getPlayListItemIndex(group) &&
        currentPlayListItem != nullptr)
    {
        auto localPosition = absoluteToLocalPosition(getAbsolutePositionClocks(), group->getPlayListContainer()->currentPlayListItem);
        auto progress = ((localPosition - currentPlayListItem->getRegionData().getStart()) / currentPlayListItem->getRegionData().getLength());
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


