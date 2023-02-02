/*
  ==============================================================================

    AudioResource.cpp
    Created: 29 Jan 2023 12:55:52pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioResource.h"




AudioResource::AudioResource(juce::URL resource, juce::AudioFormatManager& formatManager) :
    resource(resource),
    thumbnail (4096, formatManager, thumbnailCache)
{
}
