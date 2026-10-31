//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/AudiumEngine.h"
#include "Engine/ActionMessages.h"
#include "Engine/AudioSources/VoiceSourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/Undo/UndoableEdit.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/PlayList/TransportLoop.h"
#include "Engine/Group/WaveFormColours.h"
#include "Engine/Effects/MixerPlanner.h"

namespace audium {

AudioTrackContainer::~AudioTrackContainer()
{
    if (mixerPlanner != nullptr)
        mixerPlanner->setSource(nullptr);
    undoManager = nullptr;
    jassert(audioTracks.empty());
}

void AudioTrackContainer::setMixerPlanner(std::shared_ptr<MixerPlanner> planner)
{
    mixerPlanner = std::move(planner);
    masterInserts.setPlanner(mixerPlanner.get());
    if (mixerPlanner != nullptr)
        mixerPlanner->setSource(this);
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
    MixerPlanner::ScopedBatch planBatch (mixerPlanner.get());
    selectionManager->clear();
    voiceSourceContainer->cleanup();
    audioResourceContainer->cleanup();

    const auto previousNumChannels = getNumAudioTrackChannels();
    for (auto track : audioTracks)
    {
        track->cleanup();
    }
    audioTracks.clear();
    // the main mix's inserts go with the project they belong to: the plan
    // built when the batch closes has no master chain either
    masterInserts.clear();
    publishNumAudioTrackChannels();
    commitChannelLayout(0, previousNumChannels);
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
    for (const auto& track : audioTracks) {
        if (track == searchTrack)
            return numChannels;
        
        numChannels += track->getNumAudioTrackChannels();
    }
    return numChannels;
}

std::shared_ptr<AudioTrack> AudioTrackContainer::createNewAudioTrack(const juce::String nameString)
{
    MixerPlanner::ScopedBatch planBatch (mixerPlanner.get());
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
        MixerPlanner::ScopedBatch planBatch (mixerPlanner.get());
        const auto previousNumChannels = getNumAudioTrackChannels();
        const auto index = static_cast<std::size_t>(std::distance(audioTracks.begin(), it));

        track->cleanup();
        audioTracks.erase(it);
        publishNumAudioTrackChannels();
        commitChannelLayout(index, previousNumChannels);
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
    applyAsUndoableEdit(*this, [this]
    {
        for (auto track : audioTracks) {
            for (auto resourceGroup : track->getResourceGroups()) {
                resourceGroup->getAudioRegionContainer()->deleteUnusedRegions();
            }
            track->deleteUnusedResourceGroups();
        }
        return true;
    },
    "Delete Unused Regions");
}

bool AudioTrackContainer::restoreFromJson (const json& state, bool rebuild)
{
    // readFromJson may modify what it reads - the snapshot is applied again on redo
    auto input = state;
    if (readFromJson(input, rebuild)) {
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
    masterInserts.writeToJson(output, "master_inserts");
    output["loop_data"] = transportLoop->getLoopData();
    
    return true;
}

bool AudioTrackContainer::readFromJson (json& input, bool rebuild)
{
    // one plan for the whole read, after every chain and channel is in
    MixerPlanner::ScopedBatch planBatch (mixerPlanner.get());

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

    lastReadRebuiltStructure = rebuild;

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
    masterInserts.readFromJson(input, "master_inserts");
    
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

        // a track that had to replace its channels changes the structure
        // the UI is keyed on just as a replaced track does
        if (audioTrack->didLastReadRebuildChannels())
            lastReadRebuiltStructure = true;

        count++;
    }
    
    if (input.contains("loop_data")) {
        transportLoop->setLoopData(input["loop_data"].get<LoopData>());
    }
    
    return true;
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
    
    return nullptr;
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
    for (auto i = 0; i < selectedRows.size(); i++) {
        if (auto track = getAudioTrack(selectedRows[i])) {
            track->setSelected(true);
        }
    }
}

int AudioTrackContainer::getNumAudioTrackChannels() const
{
    int channels = 0;
    for (const auto& track : audioTracks) {
        channels += track->getNumAudioTrackChannels();
    }
    return channels;
}

void AudioTrackContainer::publishNumAudioTrackChannels() noexcept
{
    publishedNumAudioTrackChannels.store(getNumAudioTrackChannels(), std::memory_order_release);

    // every path that adds or removes a channel or a track comes through
    // here: the plan's channel and track lists follow. The paths open a
    // planner batch, so the plan is built and posted once, after the layout
    // they commit next, and the render thread takes both over together.
    if (mixerPlanner != nullptr)
        mixerPlanner->requestRebuild();
}

void AudioTrackContainer::commitChannelLayout(std::size_t firstTrack, int previousNumChannels)
{
    for (auto t = firstTrack; t < audioTracks.size(); ++t)
        for (auto& channel : audioTracks[t]->audioChannelContainer->getObjects())
            channel->commitChannelData();

    for (auto busChannel = getNumAudioTrackChannels(); busChannel < previousNumChannels; ++busChannel)
        audioBusInterface->setChannelData(busChannel, AudioChannelData());
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
        if(newColour == track->getViewState().getColour())
            newColour = audium::WaveFormColours::getNewWaveFormColour();
    }
    
    return newColour;
}

bool AudioTrackContainer::copySelectedChannelsToNewTrack(bool copyChannels)
{
    return applyAsUndoableEdit(*this, [this, copyChannels]
    {
        auto selectedObjects = getSelectionManager()->getSelectedObjects();
        if (selectedObjects.empty())
            return false;

        // create new audio track
        auto audioTrack = createNewAudioTrack(juce::String());
        audioTrack->getViewState().setColour(getNewAudioTrackColour());

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
        return true;
    },
    "copy channel(s)");
}

bool AudioTrackContainer::addAudioFiles(const juce::StringArray& filenames,
                                        double position,
                                        std::function<void (std::string)> callback,
                                        bool undo)
{
    auto addFiles = [&]
    {
        auto audioTrack = createNewAudioTrack(juce::String());
        audioTrack->getViewState().setColour(getNewAudioTrackColour());
        if (!audioTrack->addAudioFiles(filenames, position, callback, false)) {
            deleteAudioTrack(audioTrack.get());
            return false;
        }
        return true;
    };

    // Undo: snapshot before the new track exists, or undo leaves it behind empty
    return undo ? applyAsUndoableEdit(*this, addFiles, "File(s) added") : addFiles();
}

int AudioTrackContainer::importAudioFiles(const juce::StringArray& filenames,
                                          double position,
                                          ImportPlacement placement,
                                          std::function<void (std::string)> callback,
                                          std::shared_ptr<AudioTrack> targetTrack)
{
    if (filenames.isEmpty())
        return 0;

    juce::StringArray failed;
    auto collectError = [&failed] (std::string error) { failed.add(error); };

    // into an existing track, or into a new one that goes again if nothing landed on it
    auto trackForFiles = [this, &targetTrack]
    {
        if (targetTrack != nullptr)
            return targetTrack;

        auto audioTrack = createNewAudioTrack(juce::String());
        audioTrack->getViewState().setColour(getNewAudioTrackColour());
        return audioTrack;
    };
    auto removeIfEmpty = [this, &targetTrack] (std::shared_ptr<AudioTrack>& audioTrack, int importedOnIt)
    {
        if (importedOnIt == 0 && audioTrack != targetTrack)
            deleteAudioTrack(audioTrack.get());
    };

    // a target track always takes all files, so separate tracks don't apply there
    if (targetTrack != nullptr && placement == ImportPlacement::separateTracks)
        placement = ImportPlacement::stackedChannels;

    auto imported = 0;

    applyAsUndoableEdit(*this, [&]
    {
        if (placement == ImportPlacement::backToBack) {
            auto audioTrack = trackForFiles();
            auto clipStart = position;
            for (auto& filename : filenames) {
                if (audioTrack->addAudioFiles({filename}, clipStart, collectError, false)) {
                    ++imported;
                    // the next file starts where this clip ends; ranges are end-exclusive,
                    // so the next file gets a clip of its own instead of joining this one
                    if (auto item = audioTrack->getPlayListContainer()->itemAtAbsolutePosition(clipStart, audium::clocks))
                        clipStart = item->getAbsolutePositionRange(audium::clocks).getEnd();
                }
            }
            removeIfEmpty(audioTrack, imported);
        }
        else if (placement == ImportPlacement::stackedChannels) {
            // one file at a time, so a failing file is reported on its own: the
            // first one creates the clip, the others land on it as extra channels
            auto audioTrack = trackForFiles();
            for (auto& filename : filenames) {
                if (audioTrack->addAudioFiles({filename}, position, collectError, false))
                    ++imported;
            }
            removeIfEmpty(audioTrack, imported);
        }
        else {
            for (auto& filename : filenames) {
                auto audioTrack = trackForFiles();
                auto added = audioTrack->addAudioFiles({filename}, position, collectError, false) ? 1 : 0;
                imported += added;
                removeIfEmpty(audioTrack, added);
            }
        }
        return imported > 0;
    },
    "Import audio");

    if (!failed.isEmpty())
        NullCheckedInvocation::invoke(callback, failed.joinIntoString("\n").toStdString());

    return imported;
}

} // namespace audium

