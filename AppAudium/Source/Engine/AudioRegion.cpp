/*
  ==============================================================================

    AudioRegion.cpp
    Created: 30 May 2023 10:16:15am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioRegion.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/AudioResourceContainer.h"
#include "Engine/Factory/AudioResourceFactory.h"
#include "Engine/Provider/TempoProvider.h"

AudioRegion::~AudioRegion()
{
}

bool AudioRegion::writeToStream (juce::OutputStream& outputStream)
{
    outputStream.writeString(getName());
    outputStream.writeInt(getAudioGroup()->getId());
    outputStream.writeInt(getAudioSubGroup()->getId());
    outputStream.writeDouble(getRegionData(audium::seconds).getStart());
    outputStream.writeDouble(getRegionData(audium::seconds).getEnd());
    return true;
}

bool AudioRegion::readFromStream (juce::InputStream& inputStream)
{
    name = inputStream.readString();
    auto groupId    = inputStream.readInt();
    auto subGroupId = inputStream.readInt();
    auto start      = inputStream.readDouble();
    auto end        = inputStream.readDouble();
        
    regionData = juce::Range<double>(start, end);
    
    auto group = audioGroup->getAudioGroupContainer().getAudioGroupById(groupId);
    jassert(group == audioGroup);
    
    auto subGroup = audioGroup->getAudioSubGroupById(subGroupId);
    jassert(subGroup == audioSubGroup);

    audioGroup->getAudioGroupContainer().sendActionMessage(updateAll);
    return true;
}

int AudioRegion::getSizeInUnits()
{
    return 1;
}


const AudioRegion::RegionData AudioRegion::getRegionData(audium::TimeContextType context) const
{
    if (context == audium::seconds)
    {
        return regionData;
    }
    else if (context == audium::clocks)
    {
        return tempoProvider->secondsToClocks(regionData);
    }
    
    jassertfalse;
    return AudioRegion::RegionData();
}

void AudioRegion::setRegionData(const AudioRegion::RegionData newRegionData, audium::TimeContextType context)
{
    jassert(newRegionData.getStart() <= newRegionData.getEnd());

    if (context == audium::seconds)
    {
        regionData = newRegionData;
    }
    else if (context == audium::clocks)
    {
        regionData = tempoProvider->clocksToSeconds(newRegionData);
    }
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

double AudioRegion::getAudioResourceStartInSeconds() const
{
    double start = 0;
    for (auto resource : getAudioResources())
    {
        start = std::max(start, resource->getRegionData(audium::seconds).getStart());
    }
    return start;
}

double AudioRegion::getAudioResourceEndInSeconds() const
{
    double end = 0;
    for (auto resource : getAudioResources())
    {
        end = std::max(end, resource->getRegionData(audium::seconds).getEnd());
    }
    return end;
}

void AudioRegion::setRegionStart(double newStart, audium::TimeContextType context)
{
    if (newStart <=  getRegionData(context).getEnd())
    {
        setRegionData(AudioRegion::RegionData(newStart, getRegionData(context).getEnd()), context);
    }
}

void AudioRegion::setRegionEnd(double newEnd, audium::TimeContextType context)
{
    if (newEnd >=  getRegionData(context).getStart())
    {
        setRegionData(AudioRegion::RegionData(getRegionData(context).getStart(), newEnd), context);
    }
}

void AudioRegion::setRegionLength(double newLength, audium::TimeContextType context)
{
    setRegionData(getRegionData(context).withLength(newLength), context);
}

std::vector<std::shared_ptr<AudioResource>> AudioRegion::getAudioResources() const
{
    return audioGroup->getAudioResourceContainer().getAudioResourcesForGroupAndSubGroup(audioGroup.get(),
                                                                                        audioSubGroup.get());
}
