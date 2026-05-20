//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "PlayListItem.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/AudioSources/audium_AudioTransportSource.h"
#include "Engine/AudioSources/AudiumTransportSource.h"

namespace audium {

PlayListItem::PlayListItem(const PlayListContainer &owner_,
                           std::shared_ptr<AudioRegion> audioRegion_,
                           std::shared_ptr<SelectionManager> selectionManager_) :
audium::Selectable(selectionManager_),
owner(owner_),
audioRegion(audioRegion_)
{
    if (audioRegion != nullptr) {
        init();
    }
}

PlayListItem::~PlayListItem()
{
    deinit();
}

void PlayListItem::init()
{
    if (transportSources.size() > 0) {
        deinit();
    }

    createTransportSources();
}

void PlayListItem::createTransportSources()
{
    for (const auto &resource : getRegion()->getAudioResources()) {
        auto transportSource = owner.getAudioTrack().getAudioResourceContainer().createTransportSourceForAudioResource(resource);
        if (transportSource != nullptr)
            transportSources.emplace_back(transportSource);
    }
}

void PlayListItem::deinit()
{
    for (auto transportSource : transportSources) {
        audioRegion->getAudioTrack()->getTransportSourceContainer()->removeTransportSource(transportSource);
    }
    transportSources.clear();
}

juce::Range<double> PlayListItem::getRegionData(audium::TimeContextType context) const
{
    if (audioRegion != nullptr)
        return audioRegion->getRegionData(context);
    
    jassertfalse;
    return juce::Range<double>(0.0, 0.0);
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
    if (context == audium::seconds) {
        return owner.getTempoProvider()->clocksToSeconds(absolutePositionClocks);
    }
    else if (context == audium::clocks) {
        return absolutePositionClocks;
    }
    jassertfalse;
    return 0.0;
}

void PlayListItem::setAbsolutePosition(double newPosition, audium::TimeContextType context)
{
    if (context == audium::seconds) {
        absolutePositionClocks = owner.getTempoProvider()->secondsToClocks(newPosition);
    }
    else if (context == audium::clocks) {
        absolutePositionClocks = newPosition;
    }
    else {
        jassertfalse;
    }
}

bool PlayListItem::writeToJson (json& output)
{
    if (audioRegion != nullptr) {
        
        output["region_id"]         = audioRegion->getResourceGroup()->getAudioRegionContainer()->getRegionId(getRegion());
        output["resource_group_id"] = audioRegion->getResourceGroup()->getId();
        output["region_name"]       = getRegion()->getName().toStdString();
        output["position_clocks"]   = absolutePositionClocks;
        output["selected"]          = isSelected();
        output["track_id"]          = getRegion()->getAudioTrack()->getId();
        
        if (fadeInClocks > 0.0) {
            output["fade_in_clocks"]    = fadeInClocks;
        }
        
        if (fadeOutClocks > 0.0) {
            output["fade_out_clocks"]   = fadeOutClocks;
        }
        return true;
    }
    jassertfalse; // no audio region
    return false;
}

bool PlayListItem::readFromJson (json& input, bool rebuild)
{
    auto resourceGroupId = 0;
    if (input.contains("sub_group_id")) {
        resourceGroupId = input["sub_group_id"].template get<int>(); // lecacy
    }
    else if (input.contains("resource_group_id")) {
        resourceGroupId = input["resource_group_id"].template get<int>();
    }
    
    if (owner.getAudioTrack().resourceGroupContainer->objectExistsAtIndex(resourceGroupId)) {
        
        auto resourceGroup = owner.getAudioTrack().resourceGroupContainer->getObjects()[resourceGroupId];
        auto regionId = input["region_id"].template get<int>();
        audioRegion = resourceGroup->getAudioRegionContainer()->getRegion(regionId);
        if (audioRegion != nullptr) {
            
            init();
            
            if (input.contains("position_clocks"))
                absolutePositionClocks = input.at("position_clocks").get<double>();
            
            if (input.contains("selected"))
                setSelected(input.at("selected").get<bool>());
            
            if (input.contains("track_id")) {
                auto track_id = input.at("track_id").get<int>();
                if (track_id != getRegion()->getAudioTrack()->getId()) {
                    std::cout << "warning: track_id: " << track_id << " != " <<
                    getRegion()->getAudioTrack()->getId() << std::endl;
                }
            }
            
            if (input.contains("fade_in_clocks")) {
                fadeInClocks = input.at("fade_in_clocks").get<double>();
            }
            
            if (input.contains("fade_out_clocks")) {
                fadeOutClocks = input.at("fade_out_clocks").get<double>();
            }
        }
        return true;
    }
    
    return false;
}

bool PlayListItem::validateData()
{
    auto context = audium::clocks;
    auto position = getAbsolutePosition(context);
    
    // Absolute position must be positive
    if (position < 0.0) {
        setAbsolutePosition(0.0, context);
    }
    
    // End must NOT exeed file length
    auto totalLength = audioRegion->getResourceGroup()->getMaxLength(context);
    auto regionData = getRegionData(context);
    if (regionData.getEnd() > totalLength) {
        regionData.setEnd(totalLength);
        setRegionData(regionData, context);
    }
    
    // Start must NOT be negative
    if (regionData.getStart() < 0.0) {
        auto newPosition = position - regionData.getStart();
        regionData.setStart(0.0);
        setRegionData(regionData, context);
        setAbsolutePosition(newPosition, context);
    }
    
    return false;
}

void PlayListItem::onDragStart()
{
    undoableAction = std::make_unique<audium::UndoableContainerAction>(audioRegion->getAudioTrack()->getAudioTrackContainer(), false);
}

void PlayListItem::onDragEnd()
{
    if (undoableAction != nullptr) {
        undoableAction->storeNewState();
        auto undoManager = audioRegion->getAudioTrack()->getAudioTrackContainer().getUndoManager();
        undoManager->perform(undoableAction.release(), "Set Clip Gain");
        undoManager->beginNewTransaction();
    }
}

bool PlayListItem::setFadeIn(double val)
{
    auto length = audioRegion->getRegionData(audium::clocks).getLength();
    fadeInClocks = length * val;
    if (fadeInClocks + fadeOutClocks > length) {
        fadeOutClocks = length - fadeInClocks;
        return true;
    }
    return false;
}

double PlayListItem::getFadeIn() const
{
    auto length = audioRegion->getRegionData(audium::clocks).getLength();
    if (length > 0.0)
        return fadeInClocks / length;
    
    return 0.0;
}

bool PlayListItem::setFadeOut(double val)
{
    auto length = audioRegion->getRegionData(audium::clocks).getLength();
    fadeOutClocks = length * val;
    if (fadeInClocks + fadeOutClocks > length) {
        fadeInClocks = length - fadeOutClocks;
        return true;
    }
    return false;
}

double PlayListItem::getFadeOut() const
{
    auto length = audioRegion->getRegionData(audium::clocks).getLength();
    if (length > 0.0)
        return fadeOutClocks / length;
    
    return 0.0;
}

void PlayListItem::setClipGain(int channel, double val)
{
    audioRegion->setGain(channel, val);
}

double PlayListItem::getClipGain(int channel) const
{
    return audioRegion->getGain(channel);
}

bool PlayListItem::isRecording() const
{
    for (auto resource : audioRegion->getAudioResources()) {
        if (resource->isRecording())
            return true;
    }
    return false;
}

const double PlayListItem::getRecordedLength(audium::TimeContextType context) const
{
    auto length = 0.0;
    for (auto res : getRegion()->getAudioResources()) {
        length = std::max(res->getRecordedLength(context), length);
    }
    return length;
}

const double PlayListItem::getRecordingStartPosition(audium::TimeContextType context) const
{
    auto audioBusInterface = audioRegion->getAudioTrack()->getAudioTrackContainer().audioBusInterface;
    auto resources = getRegion()->getAudioResources();
    if (resources.size() > 0) {
        auto outChannel = resources[0]->getOutputChannelNumber();
        auto pos = audioBusInterface->getRecordingStartPosition(outChannel);
        
        
        
        if (context == audium::seconds) {
            return owner.getTempoProvider()->clocksToSeconds(pos);
        }
        else if (context == audium::clocks) {
            return pos;
        }
        
    }
    jassertfalse;
    return 0.0;
}


} // namespace audium
