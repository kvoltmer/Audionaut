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
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/AudioSources/VoiceSourceContainer.h"
#include "Engine/AudioSources/VoiceSource.h"

namespace audium {

PlayListItem::PlayListItem(PlayListContainer &owner_,
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
    if (voiceSources.size() > 0) {
        deinit();
    }

    createTransportSources();
}

void PlayListItem::createTransportSources()
{
    for (const auto &resource : getRegion()->getAudioResources()) {
        auto voiceSource = owner.getAudioTrack().getAudioResourceContainer().createTransportSourceForAudioResource(resource);
        if (voiceSource != nullptr)
            voiceSources.emplace_back(voiceSource);
    }
}

void PlayListItem::deinit()
{
    for (auto voiceSource : voiceSources) {
        audioRegion->getAudioTrack()->getVoiceSourceContainer()->removeVoiceSource(voiceSource);
    }
    voiceSources.clear();
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
    return getRegionData(context).getLength() / getSpeedRatio();
}

void PlayListItem::setSpeedRatio(double newRatio)
{
    // a still-growing recording has no stable length to scale
    if (isRecording())
        return;

    speedRatio = juce::jlimit(minSpeedRatio, maxSpeedRatio, newRatio);
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

double PlayListItem::getTailExtension(audium::TimeContextType context) const
{
    // the fade extension is source material; it rings out for
    // source / speed timeline time
    auto tailExtClocks = std::max(0.0, -dynamics.getFadeOutEnd(audium::clocks)) / getSpeedRatio();

    if (context == audium::clocks) {
        return tailExtClocks;
    }
    else if (context == audium::seconds) {
        return owner.getTempoProvider()->clocksToSeconds(tailExtClocks);
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

        // written only when set: an absent key means 1.0, and old projects
        // stay byte-identical
        if (speedRatio != 1.0)
            output["speed_ratio"] = speedRatio;

        dynamics.writeToJson(output);
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
            
            // reset first: undo replays JSON into reused item objects, and
            // an absent key must mean "no speed set", not "keep the old one"
            speedRatio = 1.0;
            if (input.contains("speed_ratio"))
                speedRatio = juce::jlimit(minSpeedRatio, maxSpeedRatio,
                                          input.at("speed_ratio").get<double>());

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
            
            dynamics.readFromJson(input);
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

void PlayListItem::onDragCancel()
{
    if (undoableAction != nullptr) {
        // restore the captured old state directly - nothing reaches the
        // undo manager, so there is nothing to redo either
        undoableAction->undo();
        undoableAction = nullptr;
    }
}

void PlayListItem::onDragEnd(const juce::String& transactionName)
{
    if (undoableAction != nullptr) {
        undoableAction->storeNewState();
        auto undoManager = audioRegion->getAudioTrack()->getAudioTrackContainer().getUndoManager();
        undoManager->perform(undoableAction.release(), transactionName);
        undoManager->beginNewTransaction();
    }
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

int PlayListItem::getId() const {
    auto item = std::dynamic_pointer_cast<const PlayListItem>(getSharedPtr());
    return owner.playListItems.getIndex(item);
}

} // namespace audium
