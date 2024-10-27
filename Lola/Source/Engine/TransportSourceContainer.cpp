/*
  ==============================================================================

    TransportSourceContainer.cpp
    Created: 14 Jun 2023 6:12:34pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "TransportSourceContainer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "AudiumTransportSource.h"

std::shared_ptr<AudiumTransportSource> TransportSourceContainer::createAndAddTransportSource(AudioResource& audioResource,
                                                                                             std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource)
{
    const ScopedLock sl (callbackLock);

    auto transportSource = std::shared_ptr<AudiumTransportSource> (new AudiumTransportSource(audioResource, audioFormatReaderSource));
    audioTransportSources.add(transportSource);
    return transportSource;
}

bool TransportSourceContainer::removeTransportSource(std::shared_ptr<AudiumTransportSource> audioTransportSource)
{
    const ScopedLock sl (callbackLock);

    if (audioTransportSources.contains(audioTransportSource)) {
        audioTransportSources.remove(getTransportSourceIndex(audioTransportSource));
        return true;
    }
    return false;
}

void TransportSourceContainer::cleanup()
{
    const ScopedLock sl (callbackLock);

    audioTransportSources.clear();
}

void TransportSourceContainer::prepareToPlay (double sampleRate, int blockSize)
{
    const ScopedLock sl (callbackLock);

    for (auto transportSource : audioTransportSources)
    {
        transportSource->prepareToPlay(blockSize, sampleRate);
    }
}

void TransportSourceContainer::startPlaying()
{
    const ScopedLock sl (callbackLock);

    playing = true;
    for (auto & transportSource : audioTransportSources)
    {
        transportSource->start();
    }
}

void TransportSourceContainer::stopPlaying()
{
    const ScopedLock sl (callbackLock);

    for (auto & transportSource : audioTransportSources)
    {

        // workaround: set the position to the very end
        if (transportSource->isPlaying())
        {
            transportSource->stopIt();
        }
    }
    playing = false;
}

bool TransportSourceContainer::isPlaying() const
{
    return playing;
}

void TransportSourceContainer::audioCallback(const juce::AudioSourceChannelInfo& info)
{
    const ScopedLock sl (callbackLock);
    
    for (auto transportSource : audioTransportSources)
    {
        const auto channels = info.buffer->getNumChannels();
        juce::AudioBuffer<float> tempBuffer(channels, info.numSamples);
        juce::AudioSourceChannelInfo tempBufferInfo (&tempBuffer, info.startSample, info.numSamples);
        if (transportSource != nullptr)
            transportSource->getNextAudioBlock(tempBufferInfo);
        
        for (auto c = 0; c < channels; c++)
        {
            info.buffer->addFrom(c, info.startSample, tempBuffer.getReadPointer(c), info.numSamples);
        }
    }
}

std::shared_ptr<AudiumTransportSource> TransportSourceContainer::getTransportSourceAtIndex(int index) const
{
    const ScopedLock sl (callbackLock);

    if (index >= 0 &&
        index < (int)audioTransportSources.size())
    {
        return audioTransportSources[index];
    }
    return nullptr;
}

int TransportSourceContainer::getTransportSourceIndex(std::shared_ptr<AudiumTransportSource> searchTransportSource) const
{
    const ScopedLock sl (callbackLock);

    int count = 0;
    for (auto transportSource : audioTransportSources)
    {
        if (transportSource == searchTransportSource)
            break;
        
        count++;
    }
    
    return count;
}

const float TransportSourceContainer::getOutputLevel(const int trackNumber, const int channelNumber) const
{
    const ScopedLock sl (callbackLock);
    
    auto level = 0.f;
    
    if (trackNumber >= 0)
    {
        for (auto transportSource : audioTransportSources)
        {
            if (trackNumber == transportSource->getAudioResource().getAudioTrack()->getId()) {
                auto channel = channelNumber - transportSource->getAudioResource().getChannelPosition();
                level += transportSource->getOutputLevel(channel);
            }
        }
    }
    
    return level;
}
