/*
  ==============================================================================

    AudioResource.cpp
    Created: 29 Jan 2023 12:55:52pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Engine/AudioResource.h"
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

double AudioResource::getLengthInSeconds() const
{
    return getAudioTransportSource()->getLengthInSeconds();
}

double AudioResource::getAbsolueStartTime() const
{    
    return getTransportPosition();
}

double AudioResource::getDurationTimeInSeconds() const
{
    if (!regionData.isEmpty())
        return regionData.getLength();
    
    return getLengthInSeconds();
}

const juce::Range<double> AudioResource::getRegionDataInSeconds() const
{
    return regionData;
}

bool approximatelyEqual(double a, double b, double epsilon)
{
    return std::abs(a - b) <= ( (std::abs(a) < std::abs(b) ? std::abs(b) : std::abs(a)) * epsilon);
}

std::vector<std::shared_ptr<AudioResource>> AudioResource::getEqualAudioResources() const
{
    std::vector<std::shared_ptr<AudioResource>> result;
    auto resources = owner.getAudioResourcesForGroup(audioGroup.get());
 
    for (auto resource : resources)
    {
        if (resource.get() == this)
            continue;
        
        if (approximatelyEqual(resource->getTransportPositionClocks(), getTransportPositionClocks(), 0.00001))
        {
            result.push_back(resource);
        }
    }
    return result;
}

void AudioResource::setRegionDataInSeconds(const juce::Range<double> newRegionData, bool syncEqualResources)
{
    jassert(newRegionData.getStart() <= newRegionData.getEnd());
    
    if (syncEqualResources)
    {
        for (auto resource : getEqualAudioResources())
            resource->setRegionDataInSeconds(newRegionData, false);
    }
    
    regionData = newRegionData;

    if (regionData.getStart() < 0.0)
    {
        regionData.setStart(0.0);
    }
}

void AudioResource::setTransportPosition(const double newPositionSeconds, bool syncEqualResources)
{
    if (syncEqualResources)
    {
        for (auto resource : getEqualAudioResources())
            resource->setTransportPosition(newPositionSeconds, false);
    }
    
    transportPositionClocks = owner.getTempoProvider()->secondsToClocks(newPositionSeconds);
}



bool AudioResource::validateData(bool syncEqualResources)
{
    bool result = false;
    
    if (syncEqualResources)
    {
        for (auto resource : getEqualAudioResources())
            result |= resource->validateData(false);
    }
    
    if (transportPositionClocks < 0.0)
    {
        transportPositionClocks = 0.0;
        result |= true;
    }
    
    if (regionData.getLength() + regionData.getStart() > getLengthInSeconds())
    {
        regionData.setLength(getLengthInSeconds() - regionData.getStart());
        result |= true;
    }
    
    if (regionData.getLength() <= 0.0)
    {
        regionData.setLength(0.1);
        result |= true;
    }
    
    
    /// TODO: check on the regions
    //    auto regions = audiumEngine->getAudioRegionContainer()->getRegionsForGroup(audioResource->getAudioGroup());
    //    for (auto region : regions)
    
    
    return result;
}

double AudioResource::getTransportPosition() const
{
    return owner.getTempoProvider()->clocksToSeconds(transportPositionClocks);
}

bool AudioResource::containsAbsolutePosition(double positionInSeconds) const
{
    auto startTime = getAbsolueStartTime();
    auto endTime = startTime + getDurationTimeInSeconds();
    juce::Range<double> absoluteRange(startTime, endTime);
    if (absoluteRange.contains(positionInSeconds))
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
    outputStream.writeInt(height);

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
    height =                    inputStream.readInt();
    
    regionData = juce::Range<double>(start, end);
    jassert(this->url == inUrl);
    getAudioTransportSource()->setGain(gain);
    
    return true;
}
