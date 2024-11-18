/*
  ==============================================================================

    TransportSourceContainer.cpp
    Created: 14 Jun 2023 6:12:34pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "TransportSourceContainer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Group/AudioTrack.h"

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

void TransportSourceContainer::prepareToPlay (int samplesPerBlockExpected,
                                              double sampleRate)
{
    const ScopedLock sl (callbackLock);

    for (auto transportSource : audioTransportSources)
    {
        transportSource->prepareToPlay(samplesPerBlockExpected, sampleRate);
    }
    
    applyChannelMapping();
}

void TransportSourceContainer::startPlaying()
{
    const ScopedLock sl (callbackLock);

    playing = true;
    for (auto & transportSource : audioTransportSources)
    {
        transportSource->getAudioTransportSource()->start();
    }
}

void TransportSourceContainer::stopPlaying()
{
    const ScopedLock sl (callbackLock);

    for (auto & transportSource : audioTransportSources)
    {
        // workaround: set the position to the very end
        if (transportSource->getAudioTransportSource()->isPlaying())
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

void TransportSourceContainer::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    const ScopedLock sl (callbackLock);
        
    // avoid reallocating
    audioBusBuffer.setSize(info.buffer->getNumChannels(), info.numSamples, false, false, true);
    audioBusBuffer.clear();
    juce::AudioSourceChannelInfo audioBusInfo (&audioBusBuffer, info.startSample, info.numSamples);
    
    for (auto transportSource : audioTransportSources) {
        if (transportSource != nullptr)
            transportSource->getNextAudioBlock(audioBusInfo);
        
        for (auto c = 0; c < info.buffer->getNumChannels(); c++)
            info.buffer->addFrom(c, info.startSample, audioBusBuffer.getReadPointer(c), info.numSamples);
    }
    
    for (auto i = 0; i < std::min(info.buffer->getNumChannels(), MAX_AUDIO_CHANNELS); i++) {
        outputLevel[i] = info.buffer->getMagnitude(i, info.startSample, info.numSamples);
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

const float TransportSourceContainer::getOutputLevel(const int channelNumber) const
{
    if (byPass)
        return 0.f;
        
    if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS)
        return outputLevel[channelNumber];
    
    return 0.f;
}

void TransportSourceContainer::applyChannelMapping()
{
    for (auto transportSource : audioTransportSources)
    {
        transportSource->applyChannelMapping();
    }
}
