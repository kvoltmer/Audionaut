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
#include "Engine/Playback/Playback.h"

std::shared_ptr<AudiumTransportSource> TransportSourceContainer::createAndAddTransportSource(AudioResource& audioResource,
                                                                                             std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource)
{
    auto transportSource = std::shared_ptr<AudiumTransportSource> (new AudiumTransportSource(audioResource, audioFormatReaderSource));
    audioTransportSources.push_back(transportSource);
    return transportSource;
}

bool TransportSourceContainer::removeTransportSource(std::shared_ptr<AudiumTransportSource> audioTransportSource)
{
    playback->stopVoice(audioTransportSource);
    
    return std::erase_if(audioTransportSources, [audioTransportSource](const auto& item) {
        return item == audioTransportSource;
    }) > 0;
}

void TransportSourceContainer::cleanup()
{
    playback->stopAllVoices();
    audioTransportSources.clear();
}

void TransportSourceContainer::prepareToPlay (int samplesPerBlockExpected,
                                              double sampleRate)
{
    for (auto transportSource : audioTransportSources)
    {
        transportSource->prepareToPlay(samplesPerBlockExpected, sampleRate);
    }
    
    applyChannelMapping();
}

std::shared_ptr<AudiumTransportSource> TransportSourceContainer::getTransportSourceAtIndex(int index) const
{
    if (index >= 0 &&
        index < (int)audioTransportSources.size())
    {
        return audioTransportSources[index];
    }
    return nullptr;
}

int TransportSourceContainer::getTransportSourceIndex(std::shared_ptr<AudiumTransportSource> searchObject) const
{
    auto it = std::find(audioTransportSources.begin(), audioTransportSources.end(), searchObject);
    if (it != audioTransportSources.end())
        return static_cast<int>(std::distance(audioTransportSources.begin(), it));

    return -1;
}

void TransportSourceContainer::applyChannelMapping()
{
    for (auto transportSource : audioTransportSources)
    {
        transportSource->applyChannelMapping();
    }
}
