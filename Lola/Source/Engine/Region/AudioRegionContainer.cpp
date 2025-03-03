/*
  ==============================================================================

    AudioRegionContainer.cpp
    Created: 30 May 2023 10:16:35am
    Author:  Klaus Voltmer

  ==============================================================================
*/
#include <iostream>

#include "AudioRegionContainer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Undo/UndoableContainerAction.h"

std::shared_ptr<AudioRegion> AudioRegionContainer::createDefaultRegion(std::shared_ptr<AudioTrack> track)
{
//    jassert(getNumRegions(track.get()) == 0);
//    auto audioResources = audioResourceContainer->getAudioResourcesForGroup(track.get());
//    auto name = (audioResources.size() > 0) ? audioResources[0]->getFileNameWithoutExtension() : "n/a";
//    auto seconds = 0.0;
//    for (auto resource : audioResources)
//    {
//        seconds = juce::jmax(seconds, resource->getAudioTransportSource()->getLengthInSeconds());
//    }
//    jassert(seconds > 0.0);
//
//    return createRegion(name, juce::Range(0.0, seconds), track);
    jassertfalse;
    return nullptr;
}

std::shared_ptr<AudioRegion> AudioRegionContainer::createRegion(std::shared_ptr<AudioTrack> track,
                                                                std::shared_ptr<AudioSubGroup> subGroup)
{
    jassert(track != nullptr);
    jassert(subGroup != nullptr);
    auto audioRegion = std::shared_ptr<AudioRegion>(new AudioRegion(track,
                                                                    subGroup,
                                                                    tempoProvider,
                                                                    track->getSelectionManager()));
    audioRegion->data.region_id = static_cast<int>(audioRegions.size());
    audioRegions.push_back(audioRegion);
    return audioRegion;
}

std::shared_ptr<AudioRegion> AudioRegionContainer::createRegion(juce::String regionName,
                                                                juce::Range<double> position,
                                                                std::shared_ptr<AudioTrack> track,
                                                                std::shared_ptr<AudioSubGroup> subGroup,
                                                                std::shared_ptr<AudioRegion> otherRegion,
                                                                audium::TimeContextType context)
{
    jassert(track != nullptr);
    
    if (subGroup == nullptr)
    {
        subGroup = track->getDefaultSubGroup();
    }
    
    auto audioRegion = createRegion(track, subGroup);
    audioRegion->setRegionData(position, context);
    audioRegion->setName(regionName);
    if (otherRegion != nullptr)
        audioRegion->data.gain_vector = otherRegion->data.gain_vector;
    return audioRegion;
}

std::shared_ptr<AudioRegion> AudioRegionContainer::createRegion(std::shared_ptr<AudioTrack> track,
                                                                std::shared_ptr<AudioSubGroup> subGroup,
                                                                const std::shared_ptr<AudioRegion> otherRegion)
{
    auto context = audium::clocks;
    
    // - create channels if needed
    if (track->getNumAudioTrackChannels() < otherRegion->getAudioTrack()->getNumAudioTrackChannels())
        track->ensureNumChannels(otherRegion->getAudioTrack()->getNumAudioTrackChannels());
    
    // - similar subgroup must exist
    jassert(track->findSimilarSubGroup(otherRegion->getAudioSubGroup()));
    
    
    // - find existing region
//    std::shared_ptr<AudioRegion> newRegion = nullptr;
//    auto regions = getRegionsForSubGroup(otherRegion->getAudioSubGroup().get());
//    for (auto region : regions) {
//        // TODO: == operator for AudioRegion
//        if (region->getRegionData(context) == otherRegion->getRegionData(context) &&
//            region->getName() == otherRegion->getName()) {
//            newRegion = region;
//            break;
//        }
//    }
    
    // - find similar region
    auto newRegion = findSimilarRegion(otherRegion);
    
    // - create region
    if (newRegion == nullptr) {
        newRegion = createRegion(track, subGroup);
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

std::shared_ptr<AudioRegion> AudioRegionContainer::getRegion(int rowNumber) const
{
    if (rowNumber >= 0 && rowNumber < audioRegions.size())
    {
        jassert(audioRegions[rowNumber]->data.region_id == rowNumber);
        return audioRegions[rowNumber];
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

int AudioRegionContainer::getNumRegions(const AudioTrack* track) const
{
    if (track == nullptr)
    {
        return static_cast<int>(audioRegions.size());
    }
    else
    {
        int count = 0;
        for (auto region : audioRegions)
        {
            if (region->getAudioTrack().get() == track)
                count++;
        }
        return count;
    }
}

std::vector<std::shared_ptr<AudioRegion>> AudioRegionContainer::getRegionsForSubGroup(const AudioSubGroup* subGroup) const
{
    std::vector<std::shared_ptr<AudioRegion>> regions;
    for (auto region : audioRegions)
    {
        if (region->getAudioSubGroup().get() == subGroup)
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

void AudioRegionContainer::deleteAudioRegionsForSubGroup(std::shared_ptr<AudioSubGroup> audioSubGroup)
{
    auto regions = getRegionsForSubGroup(audioSubGroup.get());
    
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
            object->setSelected(true, false);
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

bool AudioRegionContainer::readFromJson (json& input, bool rebuild)
{
    if (rebuild)
        cleanup();
    
    auto jsonRegions = input["regions"];
    
    for (auto& jsonElement : jsonRegions) {
        AudioRegionData regionData = jsonElement;
        
        if (auto track = audioTrackContainer.getAudioTrack(regionData.track_id)) {
            if (track->audioSubGroupContainer->objectExistsAtIndex(regionData.sub_group_id)) {
                auto subGroup = track->audioSubGroupContainer->getObjects()[regionData.sub_group_id];
                
                std::shared_ptr<AudioRegion> region = nullptr;
                if (rebuild) {
                    region = createRegion(track, subGroup);
                    region->data = regionData;
                }
                else {
                    region = getRegion(regionData.region_id);
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

void AudioRegionContainer::mergeFromJson(json& input)
{
    auto jsonRegions = input["regions"];
    
    for (auto& jsonRegion : jsonRegions) {
        AudioRegionData data = jsonRegion;
        
        auto region = getRegionWithData(data);
        
        if (region == nullptr) {
            if (auto track = audioTrackContainer.getAudioTrack(data.track_id)) {
                if (track->audioSubGroupContainer->objectExistsAtIndex(data.sub_group_id)) {
                    auto subGroup = track->audioSubGroupContainer->getObjects()[data.sub_group_id];
                    region = createRegion(track, subGroup);
                    region->data = data;
                }
            }
        }
        jassert(region);
    }
}

