//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/AudiumEngine.h"
#include "Engine/ActionMessages.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/PlayList/TransportLoop.h"

#include "Interface/ColourIds.h"

namespace audium {

AudioTrackContainer::~AudioTrackContainer()
{
    undoManager = nullptr;
    jassert(audioTracks.empty());
}

void AudioTrackContainer::setMasterGain(const float newGain)
{
    if (std::abs(newGain - masterGain) > 0.f) {
        masterGain = newGain;
        audioBusInterface->setMasterGain(newGain);
    }
}

const float AudioTrackContainer::getMasterGain() const noexcept
{
    return masterGain;
}

void AudioTrackContainer::cleanup()
{
    selectionManager->clear();
    transportSourceContainer->cleanup();
    audioResourceContainer->cleanup();
    
    for (auto track : audioTracks)
    {
        track->cleanup();
    }
    audioTracks.clear();
}

std::shared_ptr<AudioTrack> AudioTrackContainer::getAudioTrack(int index) const
{
    if (index >= 0 && index < audioTracks.size())
    {
        return audioTracks[index];
    }
    return nullptr;
}

int AudioTrackContainer::getAudioTrackId(std::shared_ptr<const AudioTrack> searchTrack) const
{
    auto it = std::find(audioTracks.begin(), audioTracks.end(), searchTrack);
    if (it != audioTracks.end())
        return static_cast<int>(std::distance(audioTracks.begin(), it));
    
    return -1; // not found
}

int AudioTrackContainer::getChannelOffset(std::shared_ptr<const AudioTrack> searchTrack) const
{
    int numChannels = 0;
    for (auto track : audioTracks) {
        if (track == searchTrack)
            return numChannels;
        
        numChannels += track->getNumAudioTrackChannels();
    }
    return numChannels;
}

std::shared_ptr<AudioTrack> AudioTrackContainer::createNewAudioTrack(const juce::String nameString)
{
    auto audioTrack = AudioTrackFactory::createAudioTrack(*this, audioResourceContainer);
    if (nameString.isEmpty())
    {
        audioTrack->setAudioTrackName(juce::String("Track ") + juce::String(audioTracks.size() + 1));
    }
    else
    {
        audioTrack->setAudioTrackName(nameString);
    }
    audioTracks.push_back(audioTrack);
    return audioTrack;
}

bool AudioTrackContainer::deleteAudioTrack(AudioTrack* track)
{
    auto it = std::find_if(audioTracks.begin(), audioTracks.end(), [track](const auto& item) {
        return item.get() == track;
    });
    
    if (it != audioTracks.end()) {
        track->cleanup();
        audioTracks.erase(it);
        return true;
    }
    
    return false;
}

bool AudioTrackContainer::deleteAudioTrack(std::shared_ptr<AudioTrack> track)
{
    return deleteAudioTrack(track.get());
}

void AudioTrackContainer::deleteSelectedObjects()
{
    // Undo: store old state
    auto action = std::make_unique<audium::UndoableContainerAction>(*this);
    auto rebuild = false;
    auto objects = selectionManager->getSelectedObjects();
    
    for (auto object : objects) {
        if (auto track = dynamic_cast<AudioTrack*>(object.get())) {
            deleteAudioTrack(track);
        }
        else {
            for (auto track : audioTracks) {
                track->deleteSelectedObject(object, rebuild);
            }
        }
    }
    
    selectionManager->clear();
    
    // Undo: store new state and perform
    action->storeNewState();
    action->rebuild = rebuild;
    undoManager->perform(action.release(), "Delete Selected Objects(s)");
    undoManager->beginNewTransaction();
    
}

void AudioTrackContainer::deleteUnusedRegions()
{
    // Undo: store old state
    auto action = std::make_unique<audium::UndoableContainerAction>(*this);
    
    for (auto track : audioTracks) {
        for (auto subGroup : track->getAudioSubGroups()) {
            subGroup->getAudioRegionContainer()->deleteUnusedRegions();
        }
        track->deleteUnusedSubGroups();
    }
    
    // Undo: store new state and perform
    action->storeNewState();
    undoManager->perform(action.release(), "Delete Unused Regions");
    undoManager->beginNewTransaction();
}

bool AudioTrackContainer::writeToStream (juce::OutputStream& outputStream)
{
    return audium::Streamable::writeToStream(outputStream);
}

bool AudioTrackContainer::readFromStream (juce::InputStream& inputStream, bool rebuild)
{
    if (audium::Streamable::readFromStream(inputStream, rebuild)) {
        // change message for UI
        sendActionMessage(rebuild ? rebuildAll : updateAll);
        
        // change message for Scheduler
        sendChangeMessage();
        return true;
    }
    return false;
}

bool AudioTrackContainer::writeToJson (json& output)
{
    for (auto& track : audioTracks) {
        json j;
        track->writeToJson(j);
        output["audio_tracks"] += j;
    }
    output["master_gain"] = getMasterGain();
    
    output["loop_data"] = transportLoop->loopData;
    
    return true;
}

bool AudioTrackContainer::readFromJson (json& input, bool rebuild)
{
    // std::cout << "AudioTrackContainer::readFromJson " << input.dump(2) << std::endl;
    json jsonTracks;
    if (input.contains("audio_tracks")) {
        jsonTracks = input["audio_tracks"];
    }
    else if (input.contains("groups")) {
        jsonTracks = input["groups"];
    }
    
    // fallback to rebuild:
    if (!rebuild && jsonTracks.size() != audioTracks.size())
        rebuild = true;
    
    if (rebuild) {
        cleanup();
        jassert(audioTracks.size() == 0);
    }
    
    if (input.contains("master_gain")) {
        setMasterGain(input.at("master_gain").get<float>());
    }
    else {
        setMasterGain(1.f);
    }
    
    
    int count = 0;
    for (auto& jsonElement : jsonTracks) {
        std::shared_ptr<AudioTrack> audioTrack = nullptr;
        if (rebuild) {
            audioTrack = AudioTrackFactory::createAudioTrack(*this, audioResourceContainer);
            audioTracks.push_back(audioTrack);
        }
        else {
            audioTrack = audioTracks[count];
        }
        
        if ( !audioTrack->readFromJson(jsonElement, rebuild))
            return false;
        
        count++;
    }
    
    if (input.contains("loop_data")) {
        transportLoop->loopData = input["loop_data"];
    }
    
    return true;
}

int AudioTrackContainer::getSizeInUnits()
{
    return getNumItems() * 8;
}

std::shared_ptr<AudioTrack> AudioTrackContainer::getDefaultGroup() const
{
    // returns the first selected track
    for (auto track : audioTracks)
    {
        if (track->isSelected())
            return track;
    }
    
    // in case nothing is selected the first track is returned
    if (audioTracks.size() > 0)
    {
        return audioTracks[0];
    }
    
    jassertfalse;
    return nullptr;
}

void AudioTrackContainer::selectAllGroups(bool bSelected, bool selectChildren)
{
    for (auto track : audioTracks)
        track->setSelected(bSelected, selectChildren);
}

juce::SparseSet<int> AudioTrackContainer::getSelectedRows() const
{
    juce::SparseSet<int> result;
    for (auto i = 0; i < getNumItems(); i++)
    {
        if (getAudioTrack(i) != nullptr &&
            getAudioTrack(i)->isSelected())
        {
            result.addRange ({i, i + 1});
        }
    }
    return result;
}

void AudioTrackContainer::setSelectedRows(juce::SparseSet<int>& selectedRows)
{
    getSelectionManager()->deselectAll();
    for (auto i = 0; i < selectedRows.size(); i++)
    {
        if (auto track = getAudioTrack(selectedRows[i]))
        {
            track->setSelected(true, false);
        }
    }
}

int AudioTrackContainer::getNumAudioTrackChannels() const
{
    int channels = 0;
    for (auto track : audioTracks) {
        channels += track->getNumAudioTrackChannels();
    }
    return channels;
}

bool AudioTrackContainer::anyChannelSolo() const
{
    for (auto track : audioTracks) {
        for (auto channel : track->audioChannelContainer->objects) {
            if (channel->getSolo())
                return true;
        }
    }
    return false;
}

juce::Colour AudioTrackContainer::getNewAudioTrackColour() const
{
    auto newColour = audium::WaveFormColours::getNewWaveFormColour();
    
    for (auto track : audioTracks) {
        if(newColour == track->getColour())
            newColour = audium::WaveFormColours::getNewWaveFormColour();
    }
    
    return newColour;
}

void AudioTrackContainer::copySelectedChannelsToNewTrack(bool copyChannels)
{
    // undo
    auto action = std::make_unique<audium::UndoableContainerAction>(*this);
    
    auto selectedObjects = getSelectionManager()->getSelectedObjects();
    if (selectedObjects.size() > 0) {
        
        // create new audio track
        auto audioTrack = createNewAudioTrack(juce::String());
        audioTrack->setColour(getNewAudioTrackColour());
        
        // copy selected channels
        for (auto object : selectedObjects) {
            
            if (auto audioChannel = std::dynamic_pointer_cast<AudioChannel>(object)) {
                json j;
                auto track = &audioChannel->getAudioTrack();
                track->writeChannelToJson(j, audioChannel.get());
                audioTrack->mergeChannelFromJson(j);
                if (!copyChannels)
                    track->deleteChannel(audioChannel.get());
            }
        }
    }
    
    
    // undo
    action->storeNewState();
    undoManager->perform(action.release(), "copy channel(s)");
    undoManager->beginNewTransaction();
}

bool AudioTrackContainer::addAudioFiles(const juce::StringArray& filenames,
                               double position,
                               bool arrangementMode,
                               std::function<void (std::string)> callback)
{
    auto audioTrack = createNewAudioTrack(juce::String());
    audioTrack->setColour(getNewAudioTrackColour());
    if (!audioTrack->addAudioFiles(filenames, position, arrangementMode, callback)) {
        deleteAudioTrack(audioTrack.get());
        return false;
    }
    return true;
}

} // namespace audium

