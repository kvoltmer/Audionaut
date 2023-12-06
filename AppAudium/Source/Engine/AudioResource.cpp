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
void AudioResource::setRegionDataInSeconds(const juce::Range<double> newRegionData)
{
    jassert(newRegionData.getStart() <= newRegionData.getEnd());
    regionData = newRegionData;

    if (regionData.getStart() < 0.0)
        regionData.setStart(0.0);
    
    if (regionData.getLength() + regionData.getStart() > getLengthInSeconds())
        regionData.setLength(getLengthInSeconds() - regionData.getStart());
}

void AudioResource::setTransportPosition(const double newPosition)
{
    transportPosition = newPosition;
    if (transportPosition < 0.0)
        transportPosition = 0.0;
}
