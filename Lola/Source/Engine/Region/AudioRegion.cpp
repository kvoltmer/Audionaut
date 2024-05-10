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
#include "Engine/Group/AudioClip.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioResourceFactory.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/PlayList/PlayListContainer.h"

AudioRegion::~AudioRegion()
{
}

bool AudioRegion::writeToStream (juce::OutputStream& outputStream)
{
    return audium::Streamable::writeToStream(outputStream);
}

bool AudioRegion::readFromStream (juce::InputStream& inputStream, bool rebuild)
{
    if (audium::Streamable::readFromStream(inputStream))
    {
        audioGroup->getAudioGroupContainer().sendActionMessage(updateAll);
        return true;
    }
    return false;
}

bool AudioRegion::writeToJson (json& output)
{
    output = data;
    return true;
}

bool AudioRegion::readFromJson (json& input, bool rebuild)
{
    data = input;
    return true;
}


int AudioRegion::getSizeInUnits()
{
    return 1;
}

void AudioRegion::setSelected(bool bSelected)
{
    data.selected = bSelected;
}

const AudioRegionData::tRange AudioRegion::getRegionData(audium::TimeContextType context) const
{
    if (context == audium::seconds)
    {
        return data.regionData;
    }
    else if (context == audium::clocks)
    {
        return tempoProvider->secondsToClocks(data.regionData);
    }
    
    jassertfalse;
    return AudioRegionData::tRange();
}

void AudioRegion::setRegionData(const AudioRegionData::tRange newRegionData, audium::TimeContextType context)
{
    jassert(newRegionData.getStart() <= newRegionData.getEnd());

    if (context == audium::seconds)
    {
        data.regionData = newRegionData;
    }
    else if (context == audium::clocks)
    {
        data.regionData = tempoProvider->clocksToSeconds(newRegionData);
    }
}

bool AudioRegion::validateData(AudioRegionData::tRange& newData, audium::TimeContextType context)
{
    bool result = false;
    
    if (newData.getStart() < getAudioResourceStart(context))
    {
        newData = newData.movedToStartAt(getAudioResourceStart(context));
        result |= true;
    }
    
    if (newData.getEnd() > getAudioResourceEnd(context))
    {
        newData = newData.movedToEndAt(getAudioResourceEnd(context));
        result |= true;
    }
    return result;
}

double AudioRegion::getAudioResourceStart(audium::TimeContextType context) const
{
    return audioSubGroup->getAudioClip()->getRegionData(context).getStart();
}

double AudioRegion::getAudioResourceEnd(audium::TimeContextType context) const
{
    return audioSubGroup->getAudioClip()->getRegionData(context).getEnd();
}

void AudioRegion::setRegionStart(double newStart, audium::TimeContextType context)
{
    if (newStart <=  getRegionData(context).getEnd())
    {
        setRegionData(AudioRegionData::tRange(newStart, getRegionData(context).getEnd()), context);
    }
}

void AudioRegion::setRegionEnd(double newEnd, audium::TimeContextType context)
{
    if (newEnd >=  getRegionData(context).getStart())
    {
        setRegionData(AudioRegionData::tRange(getRegionData(context).getStart(), newEnd), context);
    }
}

void AudioRegion::setRegionLength(double newLength, audium::TimeContextType context)
{
    setRegionData(getRegionData(context).withLength(newLength), context);
}

std::vector<std::shared_ptr<AudioResource>> AudioRegion::getAudioResources() const
{
    return audioGroup->getAudioResourceContainer().getAudioResourcesForSubGroup(audioSubGroup.get());
}

bool AudioRegion::deleteAssociatedItems()
{
    return getAudioGroup()->getPlayListContainer()->deleteAssociatedItems(this);
}
