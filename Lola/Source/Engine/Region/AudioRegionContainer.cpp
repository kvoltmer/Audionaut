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
    audioTrackContainer.sendActionMessage(regionCreatedAction);
    return audioRegion;
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

std::vector<std::shared_ptr<AudioRegion>> AudioRegionContainer::getSelectedRegions() const
{
    std::vector<std::shared_ptr<AudioRegion>> regions;
    for (auto region : audioRegions)
    {
        if (region->isSelected())
            regions.push_back(region);
    }
    return regions;
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
