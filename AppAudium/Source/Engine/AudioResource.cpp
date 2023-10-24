/*
  ==============================================================================

    AudioResource.cpp
    Created: 29 Jan 2023 12:55:52pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioResource.h"
#include "AudioResourceContainer.h"
#include "AudioPlayer.h"
#include "TransportSourceContainer.h"

AudioResource::AudioResource(AudioResourceContainer& audioResourceContainer,
                             juce::URL url,
                             juce::InputSource* inputSource,
                             juce::AudioFormatManager& formatManager,
                             std::shared_ptr<AudioPlayer> audioPlayer,
                             juce::AudioThumbnailCache& thumbnailCache) :
    owner(audioResourceContainer),
    url(url),
    thumbnail (4096, formatManager, thumbnailCache),
    audioPlayer(audioPlayer)
{
    thumbnail.setSource(inputSource);
}

AudioResource::~AudioResource()
{
    std::cout << "~AudioResource" << std::endl;
    //auto removed = owner.getTransportSourceContainer()->removeTransportSource(audioPlayer->getAudioTransportSource());
    //jassert(removed);
}


double AudioResource::getTotalLengthMax() const
{
    return owner.getTotalLengthMax();
}

std::shared_ptr<AudiumTransportSource> AudioResource::getAudioTransportSource()
{
    return audioPlayer->getAudioTransportSource();
}

const juce::String AudioResource::getFileNameWithoutExtension() const
{
    return url.getLocalFile().getFileNameWithoutExtension();
}

const juce::String AudioResource::getFullPathName() const
{
    return url.getLocalFile().getFullPathName();
}

const juce::String AudioResource::getUrlAsString() const
{
    return url.toString(true);
}

double AudioResource::getSampleRate() const
{
    return audioPlayer->sampleRate;
}

unsigned int AudioResource::getNumChannels() const
{
    return audioPlayer->numChannels;
}
