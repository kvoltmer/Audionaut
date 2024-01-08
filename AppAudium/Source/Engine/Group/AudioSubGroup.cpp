/*
  ==============================================================================

    AudioSubGroup.cpp
    Created: 19 Dec 2023 3:47:13pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioSubGroup.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/AudioResourceContainer.h"

bool AudioSubGroup::writeToStream (juce::OutputStream& outputStream)
{
    outputStream.writeInt(subGroupId);
    return true;
}

bool AudioSubGroup::readFromStream (juce::InputStream& inputStream)
{
    subGroupId = inputStream.readInt();
    return true;
}

std::vector<std::shared_ptr<AudioResource>> AudioSubGroup::getAudioResources() const
{
    return audioGroup.getAudioResourceContainer().getAudioResourcesForGroupAndSubGroup(&audioGroup, this);
}

int AudioSubGroup::getNumChannels() const
{
    return audioGroup.getNumChannels();
}

std::shared_ptr<AudioResource> AudioSubGroup::getChannel(int rowNumber) const
{
    for (auto resource : getAudioResources())
    {
        if (resource->containsChannelNumber(rowNumber))
            return resource;
    }
    return nullptr;
}

juce::Range<double> AudioSubGroup::getAbsolutePosition(audium::TimeContextType context) const
{
    // note: a sub group my contain several resources...
    auto pos = 0.0;
    auto length = 0.0;
    for (auto resource : getAudioResources())
    {
        pos = resource->getTransportPosition(context);
        length = std::max(length, resource->getRegionData(context).getEnd());
    }
    
    return juce::Range(pos, pos + length);
}
