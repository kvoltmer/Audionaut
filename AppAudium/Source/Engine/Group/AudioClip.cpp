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
#include "Engine/Resource/AudioResource.h"

juce::Range<double> AudioClip::getAbsolutePositionRange(audium::TimeContextType context) const
{
    const auto length = getRegionData(context).getLength();

    if (context == audium::seconds)
    {
        auto tp = getAudioGroup().getAudioGroupContainer().getTempoProvider();
        auto pos = tp->clocksToSeconds(data.absolutePositionClocks);
        return juce::Range(pos, pos + length);
    }
    else if (context == audium::clocks)
    {
        return juce::Range(data.absolutePositionClocks, data.absolutePositionClocks + length);
    }
    
    jassertfalse;
    return juce::Range(0.0, 0.0);
}

double AudioClip::getAbsolutePosition(audium::TimeContextType context) const
{
    if (context == audium::seconds)
    {
        auto tp = getAudioGroup().getAudioGroupContainer().getTempoProvider();
        return tp->clocksToSeconds(data.absolutePositionClocks);
    }
    else if (context == audium::clocks)
    {
        return data.absolutePositionClocks;
    }
    jassertfalse;
    return 0.0;
}


void AudioClip::setAbsolutePosition(double newPosition, audium::TimeContextType context)
{
    if (context == audium::seconds)
    {
        auto tp = getAudioGroup().getAudioGroupContainer().getTempoProvider();
        data.absolutePositionClocks = tp->secondsToClocks(newPosition);
    }
    else if (context == audium::clocks)
    {
        data.absolutePositionClocks = newPosition;
    }
    else
    {
        jassertfalse;
    }
}

const juce::Range<double> AudioClip::getRegionData(audium::TimeContextType context) const
{
    if (data.regionData.isEmpty())
    {
        return juce::Range<double>(0.0, getFileLength(context));
    }
    
    if (context == audium::seconds)
    {
        return data.regionData;
    }
    else if (context == audium::clocks)
    {
        return getAudioGroup().getAudioGroupContainer().getTempoProvider()->secondsToClocks(data.regionData);
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
        data.regionData = newRegionData;
    }
    else if (context == audium::clocks)
    {
        data.regionData = getAudioGroup().getAudioGroupContainer().getTempoProvider()->clocksToSeconds(newRegionData);
    }

    if (data.regionData.getStart() < 0.0)
    {
        data.regionData.setStart(0.0);
    }
}

bool AudioClip::validateData()
{
    bool result = false;
 
    if (data.regionData.isEmpty())
    {
        setRegionData(juce::Range<double>(0.0, getFileLength(audium::seconds)), audium::seconds);
    }
    
    if (getAbsolutePosition(audium::clocks) < 0.0)
    {
        setAbsolutePosition(0.0, audium::clocks);
        result |= true;
    }
    
    if (getFileLength(audium::seconds) > 0.0)
    {
        if (data.regionData.getLength() + data.regionData.getStart() > getFileLength(audium::seconds))
        {
            data.regionData.setLength(getFileLength(audium::seconds) - data.regionData.getStart());
            result |= true;
        }
    }
    
    if (data.regionData.getLength() <= 0.0)
    {
        data.regionData.setLength(0.1);
        result |= true;
    }
    
    return result;
}

bool AudioClip::writeToStream (juce::OutputStream& outputStream)
{
    return audium::Streamable::writeToStream(outputStream);
}

bool AudioClip::readFromStream (juce::InputStream& inputStream)
{
    if (audium::Streamable::readFromStream(inputStream))
    {
        getAudioGroup().getAudioGroupContainer().sendActionMessage(updateMiddlePanelAction);
        return true;
    }
    return false;
}

bool AudioClip::writeToJson (json& output)
{
    output = data;
    return true;
}

bool AudioClip::readFromJson (json& input)
{
    data = input;
    return true;
}

double AudioClip::getFileLength(audium::TimeContextType context) const
{
    if (audioSubGroup.getAudioResources().size() == 0)
        std::cout << "AudioClip::getFileLength -> audioSubGroup.getAudioResources().size() == 0" << std::endl;
    
    auto maxLength = 0.0;
    for (auto res : audioSubGroup.getAudioResources())
    {
        maxLength = std::max(maxLength, res->getFileLength(context));
    }
    
    return maxLength;
}
