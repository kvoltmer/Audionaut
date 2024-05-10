/*
  ==============================================================================

    PlayListItem.cpp
    Created: 28 Jun 2023 11:51:10am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "PlayListItem.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Provider/TempoProvider.h"

PlayListItem::PlayListItem(const PlayListContainer &owner, std::shared_ptr<AudioRegion> audioRegion) :
    owner(owner),
    audioRegion(audioRegion)
{
}

juce::Range<double> PlayListItem::getRegionData(audium::TimeContextType context) const
{
    return audioRegion->getRegionData(context);
}

double PlayListItem::getDurationTime(audium::TimeContextType context) const
{
    return getRegionData(context).getLength();
}

juce::Range<double> PlayListItem::getAbsolutePositionRange(audium::TimeContextType context) const
{
    const auto start = getAbsolutePosition(context);
    const auto length = getRegionData(context).getLength();
    return juce::Range<double>(start, start + length);
}

double PlayListItem::getAbsolutePosition(audium::TimeContextType context) const
{
    
    if (context == audium::seconds)
    {
        auto tp = owner.getTempoProvider();
        return tp->clocksToSeconds(absolutePositionClocks);
    }
    else if (context == audium::clocks)
    {
        return absolutePositionClocks;
    }
    jassertfalse;
    return 0.0;
}

void PlayListItem::setAbsolutePosition(double newPosition, audium::TimeContextType context)
{
    if (context == audium::seconds)
    {
        auto tp = owner.getTempoProvider();
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

void PlayListItem::moveAbsolutePosition(double amount, audium::TimeContextType context)
{
    setAbsolutePosition(getAbsolutePosition(context) + amount, context);
}

bool PlayListItem::writeToJson (json& output)
{
    output["region_id"] = owner.getAudioRegionContainer().getRegionIndex(getRegion());
    output["region_name"] = getRegion()->getName().toStdString();
    output["position_clocks"] = absolutePositionClocks;
    output["selected"] = selected;
    return true;
}

bool PlayListItem::readFromJson (json& input, bool rebuild)
{
    auto regionName = input["region_name"].template get<std::string>();
    jassert(regionName == getRegion()->getName().toStdString());

    auto regionId = input["region_id"].template get<int>();
    auto id = owner.getAudioRegionContainer().getRegionIndex(getRegion());
    jassert(id == regionId);
    
    if (input.contains("position_clocks"))
        absolutePositionClocks = input.at("position_clocks").get<double>();
    
    if (input.contains("selected"))
        selected = input.at("selected").get<bool>();

    return true;
}

bool PlayListItem::validateData()
{
    bool result = false;
    
    if (getAbsolutePosition(audium::clocks) < 0.0)
    {
        setAbsolutePosition(0.0, audium::clocks);
        result |= true;
    }
    
    return result;
}
