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
                             juce::InputSource* inputSource,
                             juce::AudioFormatManager& formatManager,
                             std::shared_ptr<AudioPlayer> audioPlayer) :
    audioResourceContainer(audioResourceContainer),
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
    return audioResourceContainer.getTotalLengthMax();
}

void AudioResource::start()
{
    audioPlayer->start();
}

void AudioResource::stop()
{
    audioPlayer->stop();
}
