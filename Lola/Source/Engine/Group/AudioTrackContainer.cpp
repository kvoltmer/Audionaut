/*
  ==============================================================================

    AudioTrackContainer.cpp
    Created: 10 Oct 2023 12:12:23pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

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

AudioTrackContainer::~AudioTrackContainer()
{
    undoManager = nullptr;
    jassert(audioTracks.empty());
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
        audioTrack->setName(juce::String("Track ") + juce::String(audioTracks.size() + 1));
    }
    else
    {
        audioTrack->setName(nameString);
    }
    audioTracks.push_back(audioTrack);
    sendActionMessage(rebuildAll);
    return audioTrack;
}

bool AudioTrackContainer::deleteAudioTrack(AudioTrack* track)
{
    auto it = std::find_if(audioTracks.begin(), audioTracks.end(), [track](const auto& item) {
        return item.get() == track;
    });
    
    if (it != audioTracks.end()) {
        track->getAudioRegionContainer()->cleanup();
        track->getAudioResourceContainer().removeAudioResourcesForTrack(track);
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

    auto objects = selectionManager->getSelectedObjects();
    
    for (auto object : objects) {
        if (auto track = dynamic_cast<AudioTrack*>(object.get())) {
            deleteAudioTrack(track);
        }
        else {
            for (auto g : audioTracks) {
                if (g->deleteSelectedObject(object))
                    continue;
            }
        }
    }
    
    selectionManager->clear();
    
    // Undo: store new state and perform
    action->storeNewState();
    undoManager->perform(action.release(), "Delete Selected Objects(s)");
    undoManager->beginNewTransaction();
    
}

void AudioTrackContainer::deleteUnusedRegions()
{
    // Undo: store old state
    auto action = std::make_unique<audium::UndoableContainerAction>(*this);

    for (auto track : audioTracks) {
        track->getAudioRegionContainer()->deleteUnusedRegions();
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
    if (audium::Streamable::readFromStream(inputStream, rebuild))
    {
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
    for (auto& track : audioTracks)
    {
        json j;
        track->writeToJson(j);
        output["groups"] += j;
    }
    return true;
}

bool AudioTrackContainer::readFromJson (json& input, bool rebuild)
{
    //std::cout << "AudioTrackContainer::readFromJson " << rebuild << std::endl;
    
    if (rebuild)
    {
        cleanup();
        jassert(audioTracks.size() == 0);
        jassert(audioResourceContainer != nullptr);
    }
    
    auto jsonGroups = input["groups"];
    int count = 0;
    for (auto& jsonElement : jsonGroups)
    {
        std::shared_ptr<AudioTrack> audioTrack = nullptr;
        if (rebuild)
        {
            audioTrack = AudioTrackFactory::createAudioTrack(*this, audioResourceContainer);
            audioTracks.push_back(audioTrack);
        }
        else
        {
            audioTrack = audioTracks[count];
        }
        
        if ( !audioTrack->readFromJson(jsonElement, rebuild))
            return false;
        
        count++;
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
