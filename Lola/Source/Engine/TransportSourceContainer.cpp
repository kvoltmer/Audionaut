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

std::shared_ptr<AudiumTransportSource> TransportSourceContainer::createAndAddTransportSource(std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource)
{
    auto transportSource = std::shared_ptr<AudiumTransportSource> (new AudiumTransportSource(audioFormatReaderSource));
    audioTransportSources.push_back(transportSource);
    return transportSource;
}

bool TransportSourceContainer::removeTransportSource(std::shared_ptr<AudiumTransportSource> audioTransportSource)
{
    auto it = std::find(audioTransportSources.begin(), audioTransportSources.end(), audioTransportSource);
    if (it != audioTransportSources.end())
    {
        audioTransportSource->setSource(nullptr);
        audioTransportSources.erase(it);
        return true;
    }
    return false;
}

void TransportSourceContainer::cleanup()
{
    audioTransportSources.clear();
}

void TransportSourceContainer::prepareToPlay (double sampleRate, int blockSize)
{
    for (auto transportSources : audioTransportSources)
    {
        transportSources->prepareToPlay(blockSize, sampleRate);
    }
}

void TransportSourceContainer::startPlaying()
{
    playing = true;
    for (auto & transportSource : audioTransportSources)
    {
        transportSource->start();
    }
}

void TransportSourceContainer::stopPlaying()
{
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
    if (index >= 0 &&
        index < (int)audioTransportSources.size())
    {
        auto it = audioTransportSources.begin();
        std::advance(it, index);
        return (*it);
    }
    return nullptr;
}

int TransportSourceContainer::getTransportSourceIndex(std::shared_ptr<AudiumTransportSource> searchTransportSource) const
{
    auto it = std::find_if(audioTransportSources.begin(), audioTransportSources.end(), [searchTransportSource](const auto& item) {
        return item == searchTransportSource;
    });
    if (it == audioTransportSources.end())
    {
        jassertfalse;
        return -1; // not found
    }
    else
    {
        auto index = std::distance(audioTransportSources.begin(), it);
        return static_cast<int>(index);
    }
}

float TransportSourceContainer::getOutputLevel(int channelNumber) const
{
    
    auto level = 0.f;
    
    for (auto transportSource : audioTransportSources)
    {
        level += transportSource->getOutputLevel(channelNumber /* - resource->getChannelPosition() */);
    }
    
    return level;
}
