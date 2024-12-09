/*
  ==============================================================================

    SelectionManager.cpp
    Created: 6 Oct 2024 11:59:59am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "SelectionManager.h"
#include "Selectable.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/Region/AudioRegionContainer.h"

namespace audium {

void SelectionManager::deselectAll() {
    auto objects = selectedObjects;
    for (auto object : objects)
        object->setSelected(false);
    
    jassert(selectedObjects.size() == 0);
}

const SelectionContextType SelectionManager::getSelectionContext() const {
    
    if (selectedObjects.size() > 0) {
        auto object = selectedObjects.front();
        if (AudioTrack* track = dynamic_cast<AudioTrack*>(object.get())) {
            return audio_track;
        }
        if (AudioSubGroup* subgroup = dynamic_cast<AudioSubGroup*>(object.get())) {
            return sub_group;
        }
        else if (AudioChannel* channel = dynamic_cast<AudioChannel*>(object.get())) {
            return audio_channel;
        }
        else if (AudioRegion* region = dynamic_cast<AudioRegion*>(object.get())) {
            return audio_region;
        }
        else if (PlayListItem* playListItem = dynamic_cast<PlayListItem*>(object.get())) {
            return play_list_item;
        }
        else {
            jassertfalse;
        }
    }
    return invalid_context;
}

void SelectionManager::copySelectedToClipboard() {
        
    json jout, json_lola;
    
//        if (AudioSubGroup* subgroup = dynamic_cast<AudioSubGroup*>(object.get())) {
//        if (AudioChannel* channel = dynamic_cast<AudioChannel*>(object.get())) {
    
    switch (auto context = getSelectionContext()) {
        case play_list_item:
            for (auto object : selectedObjects) {
                if (PlayListItem* playListItem = dynamic_cast<PlayListItem*>(object.get())) {
                    json j;
                    playListItem->writeToJson(j);
                    jout["play_list_items"] += j;
                }
            }
            break;
        case audio_track:
            for (auto object : selectedObjects) {
                if (AudioTrack* audioTrack = dynamic_cast<AudioTrack*>(object.get())) {
                    json track;
                    audioTrack->writeToJson(track);
                    jout["audio_tracks"] += track;
                }
            }
            break;

        case audio_region:
            for (auto object : selectedObjects) {
                if (AudioRegion* audioRegion = dynamic_cast<AudioRegion*>(object.get())) {
                    json j;
                    audioRegion->writeToJson(j);
                    jout["audio_regions"] += j;
                }
            }
            break;
            
        default:
            break;
    }
    
    
    json_lola["lola"] = jout;
    
    juce::String jsonText = json_lola.dump(2);
    
    std::cout << jsonText << std::endl;
    
    juce::SystemClipboard::copyTextToClipboard (jsonText);
}


bool SelectionManager::canParseFromClipboard() {
    
    try
    {
        auto txt = juce::SystemClipboard::getTextFromClipboard().toStdString();
        
        if (json::accept(txt))
        {
            json data = json::parse(txt);
            if (data.contains("lola")) {
                return true;
            }
        }
    }
    catch (std::exception &e)
    {
        std::cout << e.what() << std::endl;
    }
    return false;
}

void SelectionManager::pasteFromClipboard(std::shared_ptr<AudiumEngine> audiumEngine,
                                          bool duplicateAction) {

    try {
        json data = json::parse(juce::SystemClipboard::getTextFromClipboard().toStdString());
        std::cout << data.dump(2) << std::endl;
        
        
        if (data.contains("lola")) {
            auto lolaData = data["lola"];
            // undo
            auto action = std::make_unique<UndoableContainerAction>(*audiumEngine->getAudioTrackContainer().get());
            
            if (lolaData.contains("play_list_items")) {
                pastePlayListItems(lolaData, audiumEngine, duplicateAction);
            }
            else if (lolaData.contains("audio_tracks")) {
                auto jsonTracks = lolaData["audio_tracks"];
                
                for (auto& jsonTrack : jsonTracks) {
                    auto audioTrack = audiumEngine->getAudioTrackContainer()->createNewAudioTrack(juce::String());
                    
                    if (audioTrack->readFromJson(jsonTrack, true)) {
                        audioTrack->setColour(audiumEngine->getAudioTrackContainer()->getNewAudioTrackColour());
                        audioTrack->setAudioTrackName(audioTrack->getAudioTrackName() + "-cpy");
                    }
                }
            }
            else if (lolaData.contains("audio_regions")) {
                auto jsonRegions = lolaData["audio_regions"];
                
                for (auto& jsonRegion : jsonRegions) {
                    if (jsonRegion.contains("track_id") &&
                        jsonRegion.contains("sub_group_id")) {
                        auto track_id = jsonRegion.at("track_id").get<int>();
                        auto audioTrack = audiumEngine->getAudioTrackContainer()->getAudioTrack(track_id);
                        jassert(audioTrack);
                        if (audioTrack != nullptr) {
                            auto sub_group_id = jsonRegion.at("sub_group_id").get<int>();
                            auto subGroup = audioTrack->audioSubGroupContainer->getObjects()[sub_group_id];
                            jassert(subGroup);
                            if (subGroup != nullptr) {
                                std::string name;
                                if (jsonRegion.contains("name"))
                                    name = jsonRegion.at("name").get<std::string>();
                                auto newName = audioTrack->getAudioRegionContainer()->getUniqueName(name);
                                auto region = audioTrack->getAudioRegionContainer()->createRegion(audioTrack, subGroup);
                                region->readFromJson(jsonRegion, false);
                                region->setName(newName);
                            }
                        }
                    }
                }
            }
            
            // undo
            action->storeNewState();
            audiumEngine->getUndoManager()->perform(action.release(), "Paste Objects(s)");
            audiumEngine->getUndoManager()->beginNewTransaction();
        }
    }
    catch (std::exception &e) {
        std::cout << e.what() << std::endl;
    }
}

void SelectionManager::pastePlayListItems(const json &input,
                                          std::shared_ptr<AudiumEngine> audiumEngine,
                                          bool duplicateAction)
{
    // deselect all and select newly created item below
    deselectAll();
    
    auto jsonPlayListItems = input["play_list_items"];
    auto pos = audiumEngine->getPlayListScheduler()->getAbsolutePosition(audium::clocks);
    
    // TODO: multiple objects not supported at this time
    auto& jsonElement = jsonPlayListItems.front();
    if (jsonElement.contains("track_id")) {
        
        auto track_id = jsonElement.at("track_id").get<int>();
        auto playListContainer = audiumEngine->getAudioTrackContainer()->getAudioTrack(track_id)->getPlayListContainer();
        
        if (playListContainer != nullptr) {
            if (auto playListItem = playListContainer->createPlayListItemFromJson(jsonElement)) {
                if (duplicateAction) {
                    auto end = playListItem->getAbsolutePositionRange(audium::clocks).getEnd();
                    playListItem->setAbsolutePosition(end, audium::clocks);
                } else {
                    // TODO: check if item exists at this position
                    playListItem->setAbsolutePosition(pos, audium::clocks);
                }
                playListItem->setSelected(true);
            }
            
            playListContainer->sortByPosition();
        }
    }
}



} // namespace audium
