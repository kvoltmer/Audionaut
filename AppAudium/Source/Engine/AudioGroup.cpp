/*
  ==============================================================================

    AudioGroup.cpp
    Created: 28 Sep 2023 1:33:20pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Engine/AudioGroup.h"
#include "Engine/AudioResourceContainer.h"
#include "Engine/AudioResource.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/TransportSourceContainer.h"

AudioGroup::~AudioGroup()
{
    cleanup();
}

void AudioGroup::cleanup()
{
    playListContainer->cleanup();
    transportSourceContainer->cleanup();
}


std::vector<std::shared_ptr<AudioResource>> AudioGroup::getAudioResources()
{
    return audioResourceContainer.getAudioResourcesForGroup(this);
}

void AudioGroup::setColour(juce::Colour colour)
{
    for (auto resource : getAudioResources())
    {
        resource->currentColour = colour;
    }
}

void AudioGroup::updateColour()
{
    // same colour for all resources in group
    auto resources = getAudioResources();
    if (resources.size() > 0)
    {
        auto colour = resources[0]->currentColour;
        for (auto resource : resources)
        {
            resource->currentColour = colour;
        }
    }
}
