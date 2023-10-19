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

PlayListScheduler::PlayListScheduler(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                                     std::shared_ptr<AudioGroupContainer> audioGroupContainer) :
    audioDeviceManager(audioDeviceManager),
    audioGroupContainer(audioGroupContainer)
{
    audioDeviceManager->addAudioCallback(this);
}

PlayListScheduler::~PlayListScheduler()
{
    audioDeviceManager->removeAudioCallback(this);
}

void PlayListScheduler::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                          int totalNumInputChannels,
                                                          float* const* outputChannelData,
                                                          int totalNumOutputChannels,
                                                          int numSamples,
                                                          [[maybe_unused]] const juce::AudioIODeviceCallbackContext& context)
{
    // these should have been prepared by audioDeviceAboutToStart()...
    jassert (sampleRate > 0 && bufferSize > 0);

    tick(numSamples);
    
    // clear output
    for (int i = 0; i < totalNumOutputChannels; ++i)
        if (outputChannelData[i] != nullptr)
            juce::zeromem (outputChannelData[i], (size_t) numSamples * sizeof (float));
    
}

void PlayListScheduler::tick(int numSamples)
{
    const juce::ScopedLock sl (readLock);

    if (isPlaying())
    {
        applyAbsolutePosition(getAbsolutePosition(), false);
        
        // increment the position
        transportPositionSamples += numSamples;
        transportPositionSeconds = samplesToSeconds(transportPositionSamples);
    }
}

double PlayListScheduler::absoluteToLocalPosition(double absolutePosition, const PlayListItem* item) const
{
    auto offset = absolutePosition - item->getAbsolueStartTime();
    return offset + item->getRegionData().getStart();
}

void PlayListScheduler::applyAbsolutePosition(double pos, bool forcePosition)
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
            group->getTransportSourceContainer()->stop();
        }

        // apply position and start if needed
        if (group->getPlayListContainer()->currentPlayListItem != nullptr)
        {
            if (not group->getTransportSourceContainer()->isPlaying() || forcePosition)
            {
                group->getTransportSourceContainer()->setLocalPosition(absoluteToLocalPosition(pos, group->getPlayListContainer()->currentPlayListItem));
                if (isPlaying())
                {
                    group->getTransportSourceContainer()->start();
                }
            }
        }
    }
    

}

void PlayListScheduler::start()
{
    playing = true;
}

void PlayListScheduler::stop()
{
    playing = false;
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        auto group = audioGroupContainer->getAudioGroup(i);
        group->getTransportSourceContainer()->stop();
    }
    
}

double PlayListScheduler::getAbsolutePosition() const
{
    return transportPositionSeconds;
}

void PlayListScheduler::setAbsolutePosition(double newPosition)
{
    transportPositionSeconds = newPosition;
    transportPositionSamples = secondsToSamples(newPosition);
    applyAbsolutePosition(newPosition, true);
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
        auto localPosition = absoluteToLocalPosition(getAbsolutePosition(), group->getPlayListContainer()->currentPlayListItem);
        auto progress = ((localPosition - currentPlayListItem->getRegionData().getStart()) / currentPlayListItem->getRegionData().getLength());
        return progress;
    }

    return 0.0;
}

void PlayListScheduler::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepareToPlay (device->getCurrentSampleRate(),
                   device->getCurrentBufferSizeSamples());
}

void PlayListScheduler::prepareToPlay (double newSampleRate, int newBufferSize)
{
    sampleRate = newSampleRate;
    bufferSize = newBufferSize;
}

void PlayListScheduler::audioDeviceStopped()
{
    sampleRate = 0.0;
    bufferSize = 0;
}
