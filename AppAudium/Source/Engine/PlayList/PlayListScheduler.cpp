/*
  ==============================================================================

    PlayListScheduler.cpp
    Created: 5 Jul 2023 3:22:44pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "PlayListScheduler.h"
#include "Engine/TransportSourceProvider.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/ActionMessages.h"

PlayListScheduler::PlayListScheduler(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                                     std::shared_ptr<TransportSourceProvider> transportSourceProvider,
                                     std::shared_ptr<PlayListContainer> playListContainer) :
    audioDeviceManager(audioDeviceManager),
    transportSourceProvider(transportSourceProvider),
    playListContainer(playListContainer)
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

double PlayListScheduler::absoluteToLocalPosition(double absolutePosition, std::shared_ptr<PlayListItem> item) const
{
    auto offset = absolutePosition - item->getAbsolueStartTime();
    return offset + item->getRegionData().getStart();
}

void PlayListScheduler::applyAbsolutePosition(double pos, bool forcePosition)
{
    // lookup current play list item
    auto item = playListContainer->getPlayListItemAtPosition(pos);
    
    // assign current item and stop
    if (item != currentPlayListItem)
    {
        currentPlayListItem = item;
        transportSourceProvider->stop();
    }
    
    // apply position and start if needed
    if (currentPlayListItem != nullptr)
    {
        if (not transportSourceProvider->isPlaying() || forcePosition)
        {
            transportSourceProvider->setLocalPosition(absoluteToLocalPosition(pos, currentPlayListItem));
            if (isPlaying())
            {
                transportSourceProvider->start();
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
    transportSourceProvider->stop();
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

int PlayListScheduler::getPlayListItemIndex() const
{
    return playListContainer->getPlayListItemIndex(currentPlayListItem);
}

void PlayListScheduler::setPlayListItemIndex(int playListItemIndex)
{
    auto item = playListContainer->getPlayListItem(playListItemIndex);
        
    auto itemStartPosition = item->getAbsolueStartTime();

    setAbsolutePosition(itemStartPosition);

    if (not isPlaying())
    {
        start();
    }
}

double PlayListScheduler::getPlayListItemProgress(int playListItemIndex) const
{
    if (playListItemIndex == getPlayListItemIndex() &&
        currentPlayListItem != nullptr)
    {
        auto localPosition = absoluteToLocalPosition(getAbsolutePosition(), currentPlayListItem);
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
