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
}


double AudioResource::getTotalLengthMax() const
{
    return owner.getTotalLengthMax();
}

void AudioResource::start()
{
    audioPlayer->start();
}

void AudioResource::stop()
{
    audioPlayer->stop();
}

std::shared_ptr<juce::AudioTransportSource> AudioResource::getAudioTransportSource()
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
