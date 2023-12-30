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

AudioResource::~AudioResource()
{
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
    return audioFormatReaderSource->getAudioFormatReader()->sampleRate;
}

unsigned int AudioResource::getNumChannels() const
{
    return audioFormatReaderSource->getAudioFormatReader()->numChannels;
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

const juce::Range<double> AudioResource::getRegionData(audium::TimeContextType context) const
{
    if (context == audium::seconds)
    {
        return regionData;
    }
    else if (context == audium::clocks)
    {
        return owner.getTempoProvider()->secondsToClocks(regionData);
    }
    jassertfalse;
    return juce::Range<double>(0.0, 0.0);
}

void AudioResource::setRegionData(const juce::Range<double> newRegionData, audium::TimeContextType context)
{
    jassert(newRegionData.getStart() <= newRegionData.getEnd());
    if (context == audium::seconds)
    {
        regionData = newRegionData;
    }
    else if (context == audium::clocks)
    {
        regionData = owner.getTempoProvider()->clocksToSeconds(newRegionData);
    }

    if (regionData.getStart() < 0.0)
    {
        regionData.setStart(0.0);
    }
}

bool AudioResource::validateData()
{
    bool result = false;
    
    if (transportPositionClocks < 0.0)
    {
        transportPositionClocks = 0.0;
        result |= true;
    }
    
    if (regionData.getLength() + regionData.getStart() > getFileLength(audium::seconds))
    {
        regionData.setLength(getFileLength(audium::seconds) - regionData.getStart());
        result |= true;
    }
    
    if (regionData.getLength() <= 0.0)
    {
        regionData.setLength(0.1);
        result |= true;
    }
    
    
    /// TODO: check on the regions
    //auto regions = audiumEngine->getAudioRegionContainer()->getRegionsForResource(audioResource);
    //    for (auto region : regions)
    
    
    return result;
}

double AudioResource::getTransportPosition(audium::TimeContextType context) const
{
    if (context == audium::seconds)
    {
        return owner.getTempoProvider()->clocksToSeconds(transportPositionClocks);
    }
    return transportPositionClocks;
}

void AudioResource::setTransportPosition(const double newPosition, audium::TimeContextType context)
{
    if (context == audium::seconds)
    {
        transportPositionClocks = owner.getTempoProvider()->secondsToClocks(newPosition);
    }
    else if (context == audium::clocks)
    {
        transportPositionClocks = newPosition;
    }
    else
    {
        jassertfalse;
    }
}

bool AudioResource::containsAbsolutePosition(double position, audium::TimeContextType context) const
{
    auto startTime = getTransportPosition(context);
    auto endTime = startTime + getRegionData(context).getLength();
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
    outputStream.writeDouble(regionData.getStart());
    outputStream.writeDouble(regionData.getEnd());
    outputStream.writeDouble(transportPositionClocks);
    outputStream.writeInt(channelPosition);
    outputStream.writeInt(0);

    return true;
}

bool AudioResource::readFromStream (juce::InputStream& inputStream)
{
    const auto inUrl =          inputStream.readString();
    const auto gain =           inputStream.readFloat();
    const auto start =          inputStream.readDouble();
    const auto end =            inputStream.readDouble();
    transportPositionClocks =   inputStream.readDouble();
    channelPosition =           inputStream.readInt();
    auto unused =               inputStream.readInt();
    
    regionData = juce::Range<double>(start, end);
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
