/*
  ==============================================================================

    PlayListSchedulder.cpp
    Created: 5 Jul 2023 3:22:44pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "PlayListSchedulder.h"
#include "Engine/TransportSourceProvider.h"
#include "Engine/PlayList/PlayListContainer.h"
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

    const juce::ScopedLock sl (readLock);

    
    auto sampleOffset = 0;
    if (sampleTimer.process(numSamples, sampleOffset))
    {
        currentRegionData = playListContainer->getPlayListDataAtIndex(nextPlayListItemIndex);
        if (currentRegionData.isEmpty())
        {
            stop();
        }
        else
        {
            transportSourceProvider->setPosition(currentRegionData.getStart() - samplesToSeconds(sampleOffset));
            if (!transportSourceProvider->isPlaying())
                transportSourceProvider->start();
            
            sampleTimer.schedule(secondsToSamples(currentRegionData.getLength()));
            currentPlayListItemIndex = nextPlayListItemIndex.load();
            nextPlayListItemIndex++;
        }
        playListContainer->sendActionMessage(playListItemTriggered);
    }
    
    // clear output
    for (int i = 0; i < totalNumOutputChannels; ++i)
        if (outputChannelData[i] != nullptr)
            juce::zeromem (outputChannelData[i], (size_t) numSamples * sizeof (float));
    
}

void PlayListScheduler::start()
{
    sampleTimer.schedule();
}
void PlayListScheduler::stop()
{
    sampleTimer.invalidate();
}

void PlayListScheduler::setPlayListItemIndex(int playListItemIndex)
{
    nextPlayListItemIndex = playListItemIndex;
    currentPlayListItemIndex = playListItemIndex;
    start();
}

double PlayListScheduler::getPlayListItemProgress(int playListItemIndex) const
{
    if (playListItemIndex == getPlayListItemIndex() &&
        !currentRegionData.isEmpty())
    {
        
        auto pos = transportSourceProvider->getCurrentPosition();
        auto progress = ((pos - currentRegionData.getStart()) / currentRegionData.getLength());
        return progress;
        
    }
//    auto region = playListModel->getPlayListContainer()->getPlayListItem(itemPlaying)->getRegion()->position;
//    auto pos = playListModel->getTransportSourceProvider()->getCurrentPosition();
//    auto progress = ((pos - region.getStart()) / region.getLength());;
    //std::cout << region.getStart() << " " << region.getEnd() << " " << pos << " " << test << std::endl;
    
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
