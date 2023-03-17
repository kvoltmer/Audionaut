/*
  ==============================================================================

    AudioResource.cpp
    Created: 29 Jan 2023 12:55:52pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioResource.h"
#include "AudioResourceContainer.h"
AudioResource::AudioResource(AudioResourceContainer& audioResourceContainer,
                             juce::URL resource,
                             juce::AudioFormatManager& formatManager) :
    audioResourceContainer(audioResourceContainer),
    resource(resource),
    thumbnail (4096, formatManager, thumbnailCache)
{
}

AudioResource::~AudioResource()
{
}


double AudioResource::getTotalLengthMax() const
{
    return audioResourceContainer.getTotalLengthMax();
}
