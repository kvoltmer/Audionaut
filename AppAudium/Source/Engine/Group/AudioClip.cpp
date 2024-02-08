/*
  ==============================================================================

    AudioClip.cpp
    Created: 8 Feb 2024 4:36:53pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioClip.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/AudioResource.h"

juce::Range<double> AudioClip::getAbsolutePositionRange(audium::TimeContextType context) const
{
    const auto length = getRegionData(context).getLength();

    if (context == audium::seconds)
    {
        auto tp = getAudioGroup().getAudioGroupContainer().getTempoProvider();
        auto pos = tp->clocksToSeconds(absolutePositionClocks);
        return juce::Range(pos, pos + length);
    }
    else if (context == audium::clocks)
    {
        return juce::Range(absolutePositionClocks, absolutePositionClocks + length);
    }
    
    jassertfalse;
    return juce::Range(0.0, 0.0);
}

double AudioClip::getAbsolutePosition(audium::TimeContextType context) const
{
    if (context == audium::seconds)
    {
        auto tp = getAudioGroup().getAudioGroupContainer().getTempoProvider();
        return tp->clocksToSeconds(absolutePositionClocks);
    }
    else if (context == audium::clocks)
    {
        return absolutePositionClocks;
    }
    jassertfalse;
    return 0.0;
}


void AudioClip::setAbsolutePosition(double newPosition, audium::TimeContextType context)
{
    if (context == audium::seconds)
    {
        auto tp = getAudioGroup().getAudioGroupContainer().getTempoProvider();
        absolutePositionClocks = tp->secondsToClocks(newPosition);
    }
    else if (context == audium::clocks)
    {
        absolutePositionClocks = newPosition;
    }
    else
    {
        jassertfalse;
    }
}

const juce::Range<double> AudioClip::getRegionData(audium::TimeContextType context) const
{
    if (regionData.isEmpty())
    {
        return juce::Range<double>(0.0, getFileLength(context));
    }
    
    if (context == audium::seconds)
    {
        return regionData;
    }
    else if (context == audium::clocks)
    {
        return getAudioGroup().getAudioGroupContainer().getTempoProvider()->secondsToClocks(regionData);
    }
    jassertfalse;
    return juce::Range<double>(0.0, 0.0);
}

void AudioClip::setRegionData(const juce::Range<double> newRegionData, audium::TimeContextType context)
{
    jassert(!newRegionData.isEmpty());
    jassert(newRegionData.getStart() <= newRegionData.getEnd());
    if (context == audium::seconds)
    {
        regionData = newRegionData;
    }
    else if (context == audium::clocks)
    {
        regionData = getAudioGroup().getAudioGroupContainer().getTempoProvider()->clocksToSeconds(newRegionData);
    }

    if (regionData.getStart() < 0.0)
    {
        regionData.setStart(0.0);
    }
}

bool AudioClip::validateData()
{
    bool result = false;
 
    if (regionData.isEmpty())
    {
        setRegionData(juce::Range<double>(0.0, getFileLength(audium::seconds)), audium::seconds);
    }
    
    if (getAbsolutePosition(audium::clocks) < 0.0)
    {
        setAbsolutePosition(0.0, audium::clocks);
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
    
    return result;
}

bool AudioClip::writeToStream (juce::OutputStream& outputStream)
{
    outputStream.writeDouble(absolutePositionClocks);
    outputStream.writeDouble(regionData.getStart());
    outputStream.writeDouble(regionData.getEnd());
    return true;
}

bool AudioClip::readFromStream (juce::InputStream& inputStream)
{
    absolutePositionClocks  = inputStream.readDouble();
    const auto start        = inputStream.readDouble();
    const auto end          = inputStream.readDouble();
    
    regionData = juce::Range<double>(start, end);
    
    getAudioGroup().getAudioGroupContainer().sendActionMessage(updateMiddlePanelAction);
    
    return true;
}

double AudioClip::getFileLength(audium::TimeContextType context) const
{
    auto maxLength = 0.0;
    for (auto res : audioSubGroup.getAudioResources())
    {
        maxLength = std::max(maxLength, res->getFileLength(context));
    }
    return maxLength;
}
