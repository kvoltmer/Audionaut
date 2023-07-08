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

    if (isPlaying())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            jassert(samplesUntilNextEvent >= 0);
            if (samplesUntilNextEvent == 0)
            {
                currentRegionData = playListContainer->getPlayListDataAtIndex(nextPlayListItemIndex);
                if (!currentRegionData.isEmpty())
                {
                    auto offset = samplesToSeconds(i);
                    transportSourceProvider->setPosition(currentRegionData.getStart() + offset);
                    if (!transportSourceProvider->isPlaying())
                    {
                        transportSourceProvider->start();
                    }
                    
                    auto length = currentRegionData.getLength();
                    samplesUntilNextEvent = secondsToSamples(length);
                    std::cout << "playing index " << nextPlayListItemIndex << " length " << length << std::endl;
                    nextPlayListItemIndex++;
                }
                else
                {
                    stop();
                    std::cout << "EOF" << std::endl;
                }
            }
            
            // sample tick
            samplesUntilNextEvent--;
        }
    }
    

    
    // clear output
    for (int i = 0; i < totalNumOutputChannels; ++i)
        if (outputChannelData[i] != nullptr)
            juce::zeromem (outputChannelData[i], (size_t) numSamples * sizeof (float));
    
}

void PlayListScheduler::start()
{
    playing = true;
}
void PlayListScheduler::stop()
{
    playing = false;
}

void PlayListScheduler::setPlayListItemIndex(int playListItemIndex)
{
    nextPlayListItemIndex = playListItemIndex;
    samplesUntilNextEvent = 0;
    start();
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
