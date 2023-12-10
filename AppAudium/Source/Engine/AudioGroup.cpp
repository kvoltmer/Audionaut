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
#include "Engine/PlayList/PlayListItem.h"
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


std::vector<std::shared_ptr<AudioResource>> AudioGroup::getAudioResources() const
{
    return audioResourceContainer.getAudioResourcesForGroup(const_cast<AudioGroup*>(this));
}

std::vector<std::shared_ptr<AudioResource>> AudioGroup::getAudioResourcesAtChannelPosition(int channelPosition) const
{
    return audioResourceContainer.getAudioResourcesForGroupAtChannelPosition(const_cast<AudioGroup*>(this), channelPosition);
}

std::vector<std::shared_ptr<AudioResource>> AudioGroup::getAudioResourcesAtAbsoluteRange(juce::Range<double> rangeInSeconds) const
{
    return audioResourceContainer.getAudioResourcesForGroupAtAbsoluteRange(const_cast<AudioGroup*>(this), rangeInSeconds);
}

void AudioGroup::setColour(juce::Colour colour)
{
    currentColour = colour;
}


bool AudioGroup::writeToStream (juce::OutputStream& outputStream)
{
    outputStream.writeInt(groupId);
    outputStream.writeString(getName());
    outputStream.writeString(currentColour.toString());
    return true;
}

bool AudioGroup::readFromStream (juce::InputStream& inputStream)
{
    groupId         = inputStream.readInt();
    groupName       = inputStream.readString();
    currentColour   = juce::Colour::fromString(inputStream.readString());
    return true;
}

int AudioGroup::getNumChannels() const
{
    int numChannels = 0;
    
    auto resources = getAudioResources();
    for (auto resource : resources)
    {
        numChannels = std::max(numChannels, static_cast<int>(resource->getChannelPosition() + resource->getNumChannels()));
    }
    
    return numChannels;
}
