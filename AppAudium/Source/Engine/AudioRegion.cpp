/*
  ==============================================================================

    AudioRegion.cpp
    Created: 30 May 2023 10:16:15am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioRegion.h"
#include "Engine/AudioGroup.h"
#include "Engine/AudioResourceContainer.h"
#include "Engine/Factory/AudioResourceFactory.h"
#include "Engine/Provider/TempoProvider.h"

AudioRegion::~AudioRegion()
{
}

void AudioRegion::setRegionData(const RegionData newRegionData)
{
    setRegionDataInSeconds(tempoProvider->clocksToSeconds(newRegionData));
}

const AudioRegion::RegionData AudioRegion::getRegionData() const
{
    return tempoProvider->secondsToClocks(regionData);
}

void AudioRegion::setRegionDataInSeconds(const RegionData newRegionData)
{
    jassert(newRegionData.getStart() <= newRegionData.getEnd());
    
    regionData = newRegionData;
}

bool AudioRegion::validateData(RegionData& data)
{
    bool result = false;
    
    if (data.getStart() < getAudioResourceStartInSeconds())
    {
        data = data.movedToStartAt(getAudioResourceStartInSeconds());
        result |= true;
    }
    
    if (data.getEnd() > getAudioResourceEndInSeconds())
    {
        data = data.movedToEndAt(getAudioResourceEndInSeconds());
        result |= true;
    }
    return result;
}

const AudioRegion::RegionData AudioRegion::getRegionDataInSeconds() const
{
    return regionData;
}

double AudioRegion::getAudioResourceStartInSeconds() const
{
    return audioResource->getRegionDataInSeconds().getStart();
}

double AudioRegion::getAudioResourceEndInSeconds() const
{
    return audioResource->getRegionDataInSeconds().getStart();
}

void AudioRegion::setRegionStart(double newStart)
{
    if (newStart <=  getRegionData().getEnd())
    {
        setRegionData(AudioRegion::RegionData(newStart, getRegionData().getEnd()));
    }
}

void AudioRegion::setRegionEnd(double newEnd)
{
    if (newEnd >=  getRegionData().getStart())
    {
        setRegionData(AudioRegion::RegionData(getRegionData().getStart(), newEnd));
    }
}

void AudioRegion::setRegionLength(double newLength)
{
    setRegionData(getRegionData().withLength(newLength));
}
