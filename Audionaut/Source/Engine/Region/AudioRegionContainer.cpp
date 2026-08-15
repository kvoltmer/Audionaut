//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <iostream>

#include "AudioRegionContainer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/Channel/AudioChannel.h"

namespace audium {

AudioRegionContainer::AudioRegionContainer(AudioTrack &audioTrack_) :
    audioTrack(audioTrack_),
    audioResourceContainer(audioTrack.getAudioResourceContainer()),
    audioTrackContainer(audioTrack.getAudioTrackContainer()),
    tempoProvider(audioTrackContainer.getTempoProvider()),
    undoManager(audioTrackContainer.getUndoManager())
{
}

std::shared_ptr<AudioRegion> AudioRegionContainer::createRegion(std::shared_ptr<AudioTrack> track,
                                                                std::shared_ptr<ResourceGroup> resourceGroup)
{
    jassert(track != nullptr);
    jassert(resourceGroup != nullptr);
    auto audioRegion = std::shared_ptr<AudioRegion>(new AudioRegion(track,
                                                                    resourceGroup,
                                                                    tempoProvider,
                                                                    track->getSelectionManager()));
    audioRegion->data.region_id = static_cast<int>(audioRegions.size());
    audioRegions.push_back(audioRegion);
    return audioRegion;
}

std::shared_ptr<AudioRegion> AudioRegionContainer::createRegion(juce::String regionName,
                                                                juce::Range<double> position,
                                                                std::shared_ptr<AudioTrack> track,
                                                                std::shared_ptr<ResourceGroup> resourceGroup,
                                                                std::shared_ptr<AudioRegion> otherRegion,
                                                                audium::TimeContextType context)
{
    jassert(track != nullptr);
    
    if (resourceGroup == nullptr) {
        resourceGroup = track->getDefaultResourceGroup();
    }
    
    auto audioRegion = createRegion(track, resourceGroup);
    audioRegion->setRegionData(position, context);
    audioRegion->setName(regionName);
    if (otherRegion != nullptr)
        audioRegion->data.gain_vector = otherRegion->data.gain_vector;
    return audioRegion;
}

std::shared_ptr<AudioRegion> AudioRegionContainer::createRegion(std::shared_ptr<AudioTrack> track,
                                                                std::shared_ptr<ResourceGroup> resourceGroup,
                                                                const std::shared_ptr<AudioRegion> otherRegion)
{
    
    // - create channels if needed
    if (track->getNumAudioTrackChannels() < otherRegion->getAudioTrack()->getNumAudioTrackChannels())
        track->ensureNumChannels(otherRegion->getAudioTrack()->getNumAudioTrackChannels());
    
    // TODO: not sure if this assertion still makes sense
#if 0
    // - similar resource group must exist
    json otherGroup;
    otherRegion->getResourceGroup()->writeToJson(otherGroup);
    jassert(track->findExistingResourceGroup(otherGroup));
#endif
    
    // - find similar region
    auto newRegion = findSimilarRegion(otherRegion);
    
    // - create region
    if (newRegion == nullptr) {
        newRegion = createRegion(track, resourceGroup);
        newRegion->data = otherRegion->data;
    }
    return newRegion;
}

std::string AudioRegionContainer::formatNumber(long num)
{
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << num;
    return oss.str();
}

const juce::String AudioRegionContainer::getUniqueName(juce::String regionName) const
{
    // - remove digits and '-' at end:
    std::string regionNameStd = regionName.toStdString();
    size_t last_index = regionNameStd.find_last_not_of("0123456789-");
    regionNameStd = regionNameStd.substr(0, last_index + 1);
    
    auto counter = 0;
    auto trail = 0;
    for (auto region : audioRegions) {
        if (region->getName().contains(juce::String(regionNameStd))) {
            trail = std::max(trail, std::abs(region->getName().getTrailingIntValue()));
            counter++;
        }
    }
    
    if (counter == 0)
        counter = 1;
    
    auto newNumber = std::max(trail + 1, counter);
    
    return regionNameStd + "-" + formatNumber(newNumber);
}

void AudioRegionContainer::cleanup()
{
    for (auto region : audioRegions)
    {
        region->deleteAssociatedItems();
    }
    
    audioRegions.clear();
}

std::shared_ptr<AudioRegion> AudioRegionContainer::getRegion(int regionId) const
{
    if (regionId >= 0 && regionId < audioRegions.size()) {
        //jassert(audioRegions[regionId]->data.region_id == regionId);
        return audioRegions[regionId];
    }
    return nullptr;
}

int AudioRegionContainer::getRegionId(std::shared_ptr<AudioRegion> searchRegion) const
{
    auto it = std::find(audioRegions.begin(), audioRegions.end(), searchRegion);
    if (it != audioRegions.end())
    {
        return static_cast<int>(std::distance(audioRegions.begin(), it));
    }
    
    jassertfalse;
    return -1; // not found
}

int AudioRegionContainer::getNumRegions() const
{
    return static_cast<int>(audioRegions.size());
}

std::vector<std::shared_ptr<AudioRegion>> AudioRegionContainer::getRegionsForResourceGroup(const ResourceGroup* resourceGroup) const
{
    std::vector<std::shared_ptr<AudioRegion>> regions;
    for (auto region : audioRegions)
    {
        if (region->getResourceGroup().get() == resourceGroup)
            regions.push_back(region);
    }
    return regions;
}

std::vector<std::shared_ptr<AudioRegion>> AudioRegionContainer::getSelectedRegions(bool global) const
{
    std::vector<std::shared_ptr<AudioRegion>> result;
    
    if (global) {
        auto selectedObjects = audioTrackContainer.getSelectionManager()->getSelectedObjects();
        
        for (auto object : selectedObjects) {
            if (auto region = std::dynamic_pointer_cast<AudioRegion>(object)) {
                jassert(region->isSelected());
                result.push_back(region);
            }
        }
    }
    else {
        for (auto r : audioRegions) {
            if (r->isSelected()) {
                result.push_back(r);
            }
        }
    }
    return result;
}

std::shared_ptr<AudioRegion> AudioRegionContainer::findSimilarRegion(std::shared_ptr<AudioRegion> otherRegion) const
{
    auto context = audium::seconds;
    for (auto region : audioRegions) {
        if (region->getRegionData(context) == otherRegion->getRegionData(context) &&
            region->getName() == otherRegion->getName()) {
            return region;
        }
    }
    return nullptr;
}

void AudioRegionContainer::deleteAudioRegionsForResourceGroup(std::shared_ptr<ResourceGroup> resourceGroup)
{
    auto regions = getRegionsForResourceGroup(resourceGroup.get());
    
    for (auto region : regions)
    {
        deleteAudioRegion(region);
    }
}

void AudioRegionContainer::deleteAudioRegion(std::shared_ptr<AudioRegion> region)
{
    deleteAudioRegion(region.get());
}

bool AudioRegionContainer::deleteAudioRegion(AudioRegion* region) {
    auto it = std::find_if(audioRegions.begin(), audioRegions.end(), [region](const auto& item) {
        return item.get() == region;
    });
    
    if (it != audioRegions.end()) {
        region->deleteAssociatedItems();
        audioRegions.erase(it);
        sortRegionIds();
        return true;
    }
    
    
    return false;
}

void AudioRegionContainer::deleteUnusedRegions()
{
    std::vector<std::shared_ptr<AudioRegion>> deleteList;
    for (auto region : audioRegions) {
        if (not region->getAudioTrack()->getPlayListContainer()->exitsInPlayList(region.get())) {
            deleteList.push_back(region);
        }
    }
    
    for (auto region : deleteList) {
        std::cout << "delete: " << region->getAudioTrack()->getAudioTrackName() << " " << region->getName() << std::endl;
        deleteAudioRegion(region);
    }
    
}

void AudioRegionContainer::sortRegionIds()
{
    auto counter = 0;
    for (auto region : audioRegions) {
        region->data.region_id = counter++;
    }
}

std::vector<std::shared_ptr<AudioRegion>> AudioRegionContainer::getRegionsForResource(std::shared_ptr<AudioResource> audioResource) const
{
    std::vector<std::shared_ptr<AudioRegion>> result;
    for (auto region : audioRegions)
    {
        auto audioResources = region->getAudioResources();
        for (auto resource : audioResources)
        {
            if (resource == audioResource)
            {
                result.push_back(region);
            }
        }
    }
    return result;
}


juce::SparseSet<int> AudioRegionContainer::getSelectedRows() const {
    juce::SparseSet<int> result;
    for (auto i = 0; i < audioRegions.size(); i++) {
        if (audioRegions[i] != nullptr &&
            audioRegions[i]->isSelected()) {
            result.addRange ({i, i + 1});
        }
    }
    return result;
}

void AudioRegionContainer::setSelectedRows(juce::SparseSet<int>& selectedRows) {
    
    
    for (auto region : audioRegions)
        region->setSelected(false);
    
    for (auto i = 0; i < selectedRows.size(); i++)
    {
        if (auto object = audioRegions[selectedRows[i]])
        {
            object->setSelected(true);
        }
    }
}

std::shared_ptr<AudioRegion> AudioRegionContainer::getRegionWithData(const AudioRegionData &data) const
{
    for (auto region : audioRegions) {
        if (region->data.range == data.range &&
            region->data.name == data.name) {
            return region;
        }
    }
    return nullptr;
}

bool AudioRegionContainer::writeToJson (json& output)
{
    for (auto region : audioRegions) {
        json r;
        region->writeToJson(r);
        output["regions"] += r;
    }
    return true;
}

bool AudioRegionContainer::writeChannelToJson(json& output, AudioChannel* audioChannel)
{
    auto channelNumber = audioChannel->getChannelNumber();
    for (auto region : audioRegions) {
        json r;
        region->writeToJson(r);
        
        // erase gain_vector
        r.erase("gain_vector");
        
        // write gain_vector for channel
        if (channelNumber >= 0 && channelNumber < region->data.gain_vector.size()) {
            std::vector<double> gain;
            gain.push_back(region->data.gain_vector[static_cast<std::size_t>(channelNumber)]);
            r["gain_vector"] = gain;
        }
        //std::cout << "writeToJson region " << r.dump(2) << std::endl;
        
        output["regions"] += r;
    }
    return true;
}

bool AudioRegionContainer::readFromJson (json& input, bool rebuild)
{
    auto jsonRegions = input["regions"];
    
    if (!rebuild && jsonRegions.size() != getNumRegions())
        rebuild = true;
    
    if (rebuild)
        cleanup();
    
    for (auto& jsonElement : jsonRegions) {
        AudioRegionData regionData = jsonElement;
        
        if (auto track = audioTrackContainer.getAudioTrack(regionData.track_id)) {
            if (track->resourceGroupContainer->objectExistsAtIndex(regionData.resource_group_id)) {
                auto resourceGroup = track->resourceGroupContainer->getObjects()[regionData.resource_group_id];
                
                std::shared_ptr<AudioRegion> region = nullptr;
                if (rebuild) {
                    region = createRegion(track, resourceGroup);
                    region->data = regionData;
                }
                else {
                    region = getRegion(regionData.region_id);
                    if (region != nullptr) {
                        region->data = regionData;
                    }
                    else {
                        jassertfalse;
                    }
                }
                
                // in case the region was in recording state we must assume the file length as region length
                if (region->data.recording) {
                    // region start should be 0.0 since we start recording at 0.0
                    jassert(region->getRegionData(audium::seconds).getStart() >= 0.0);
                    auto newLength = region->getResourceGroup()->getMaxLength(audium::seconds);
                    region->setRegionEnd(newLength, audium::seconds);
                    std::cout << "AudioRegionContainer::readFromJson region length corrected to " << newLength << std::endl;
                    
                }
                
                if (region->data.region_id != regionData.region_id) {
                    std::cout << "warning region id " << regionData.region_id << std::endl;
                    region->data.region_id = regionData.region_id;
                }
            }
        }
    }
    return true;
}

void AudioRegionContainer::mergeFromJson(json& input, int destinationChannel)
{
    auto jsonRegions = input["regions"];
    
    for (auto& jsonRegion : jsonRegions) {
        AudioRegionData data = jsonRegion;
        
        auto region = getRegionWithData(data);
        
        auto clipGain = data.gain_vector.size() > 0 ? data.gain_vector[0] : 1.0;
        
        if (region == nullptr) {
            if (auto track = std::dynamic_pointer_cast<AudioTrack>(audioTrack.getSharedPtr())) {
                if (track->resourceGroupContainer->objectExistsAtIndex(data.resource_group_id)) {
                    auto resourceGroup = track->resourceGroupContainer->getObjects()[data.resource_group_id];
                    region = createRegion(track, resourceGroup);
                    region->data = data;
                }
            }
        }
        
        jassert(region);
        
        region->setGain(destinationChannel, clipGain);
        
    }
}

} // namespace audium
