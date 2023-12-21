/*
  ==============================================================================

    AudioRegion.cpp
    Created: 30 May 2023 10:16:15am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioRegion.h"
#include "Engine/Group/AudioGroup.h"
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
    double start = 0;
    for (auto resource : getAudioResources())
    {
        start = std::min(start, resource->getRegionDataInSeconds().getStart());
    }
    return start;
}

double AudioRegion::getAudioResourceEndInSeconds() const
{
    double end = 0;
    for (auto resource : getAudioResources())
    {
        end = std::max(end, resource->getRegionDataInSeconds().getEnd());
    }
    return end;
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

std::vector<std::shared_ptr<AudioResource>> AudioRegion::getAudioResources() const
{
    return audioGroup->getAudioResourceContainer().getAudioResourcesForGroupAndSubGroup(audioGroup.get(),
                                                                                        audioSubGroup.get());
}
