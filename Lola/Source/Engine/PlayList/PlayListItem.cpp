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
#include "Engine/Resource/AudioResource.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/TransportSourceContainer.h"

PlayListItem::PlayListItem(const PlayListContainer &owner,
                           std::shared_ptr<AudioRegion> audioRegion,
                           std::shared_ptr<audium::SelectionManager> selectionManager) :
    audium::Selectable(selectionManager),
    owner(owner),
    audioRegion(audioRegion)
{
    for (const auto &resource : getRegion()->getAudioResources())
    {
        auto transportSource = owner.getAudioRegionContainer().getAudioResourceContainer().createTransportSourceForAudioResource(resource);
        transportSources.push_back(transportSource);
    }
}

PlayListItem::~PlayListItem()
{
    for (auto transportSource : transportSources)
    {
        audioRegion->getAudioGroup()->getTransportSourceContainer()->removeTransportSource(transportSource);
    }
}

juce::Range<double> PlayListItem::getRegionData(audium::TimeContextType context) const
{
    return audioRegion->getRegionData(context);
}

void PlayListItem::setRegionData(juce::Range<double> newRegionData, audium::TimeContextType context)
{
    audioRegion->setRegionData(newRegionData, context);
}

double PlayListItem::getDurationTime(audium::TimeContextType context) const
{
    return getRegionData(context).getLength();
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
    output["selected"] = isSelected();
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
        setSelected(input.at("selected").get<bool>());

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
