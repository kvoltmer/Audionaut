/*
  ==============================================================================

    AudioResource.cpp
    Created: 29 Jan 2023 12:55:52pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Engine/AudioResource.h"
#include "Engine/AudiumEngine.h"
#include "Engine/AudioResourceContainer.h"
#include "Engine/TransportSourceContainer.h"
#include "Engine/AudiumTransportSource.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Group/AudioClip.h"

AudioResource::~AudioResource()
{
    audioChannels.clear();
    transportSource->setSource(nullptr);
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
    return getAudioFormatReader()->sampleRate;
}

unsigned int AudioResource::getNumChannels() const
{
    return getAudioFormatReader()->numChannels;
}

double AudioResource::getFileLength(audium::TimeContextType context) const
{
    if (context == audium::seconds)
    {
        return getAudioTransportSource()->getLengthInSeconds();
    }
    else if (context == audium::clocks)
    {
        return owner.getTempoProvider()->secondsToClocks(getAudioTransportSource()->getLengthInSeconds());
    }
    return 0.0;
}

std::vector<std::shared_ptr<AudioResource>> AudioResource::getAudioResourcesWithinSubGroup() const
{
    std::vector<std::shared_ptr<AudioResource>> result;
    auto resources = owner.getAudioResourcesForGroupAndSubGroup(audioGroup.get(), audioSubGroup.get());
 
    for (auto resource : resources)
    {
        if (resource.get() == this)
            continue;
    
        result.push_back(resource);
    }
    return result;
}

bool AudioResource::containsAbsolutePosition(double position, audium::TimeContextType context) const
{
    auto startTime = getAudioSubGroup()->getAudioClip()->getAbsolutePosition(context);
    auto endTime = startTime + getAudioSubGroup()->getAudioClip()->getRegionData(context).getLength();
    juce::Range<double> absoluteRange(startTime, endTime);
    if (absoluteRange.contains(position))
    {
        return true;
    }

    return false;
}

bool AudioResource::writeToStream (juce::OutputStream& outputStream)
{
    outputStream.writeString(getUrlAsString());
    outputStream.writeFloat(getAudioTransportSource()->getGain());
    outputStream.writeInt(getChannelPosition());

    return true;
}

bool AudioResource::readFromStream (juce::InputStream& inputStream)
{
    const auto inUrl        = inputStream.readString();
    const auto gain         = inputStream.readFloat();
    const auto channelPos   = inputStream.readInt();
    setChannelPosition(channelPos);
    jassert(this->url == inUrl);
    getAudioTransportSource()->setGain(gain);
    
    return true;
}

void AudioResource::setSelected(bool bSelected, bool deselectOthers)
{
    if (deselectOthers)
        owner.deselectAllResources();

    selected = bSelected;
}


bool AudioResource::containsChannelNumber(int channelNumber) const
{
    juce::Range<int> channelRange(getChannelPosition(),
                                  getChannelPosition() + getNumChannels());
    
    if (channelRange.contains(channelNumber))
    {
        return true;
    }
    return false;
}

int AudioResource::getChannelPosition() const
{
    if (audioChannels.size() > 0)
    {
        return audioChannels[0]->getChannelNumber();
    }
    
    return 0;
}

void AudioResource::setChannelPosition(int startChannel)
{
    std::cout << "AudioResource::setChannelPosition " << startChannel << std::endl;
    auto numChannels = getNumChannels();
    audioChannels.clear();
    audioGroup->ensureNumChannels(startChannel + numChannels);
    
    for (auto i = startChannel; i < audioGroup->getNumChannels(); i++)
    {
        auto channel = audioGroup->getChannel(i);
        audioChannels.push_back(channel);
    }
}

bool AudioResource::deleteChannel(std::shared_ptr<AudioChannel> channel)
{
    auto it = std::find(audioChannels.begin(), audioChannels.end(), channel);
    if (it != audioChannels.end())
    {
        audioChannels.erase(it);
    }
    
    return audioChannels.size() == 0;
}
