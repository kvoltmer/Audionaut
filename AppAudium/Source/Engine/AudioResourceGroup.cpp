/*
  ==============================================================================

    AudioResourceGroup.cpp
    Created: 28 Sep 2023 1:33:20pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioResourceGroup.h"
#include "AudioResourceContainer.h"
#include "AudioResource.h"

std::vector<std::shared_ptr<AudioResource>> AudioResourceGroup::getAudioResources()
{
    return owner.getAudioResourcesForGroup(shared_from_this());
}

void AudioResourceGroup::setColour(juce::Colour colour)
{
    auto resources = getAudioResources();
    for (auto resource : resources)
    {
        resource->currentColour = colour;
    }
}
