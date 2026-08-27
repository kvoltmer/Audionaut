//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ProjectSerializer.h"
#include "ProjectFileStore.h"
#include "Engine/ActionMessages.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Analysis/AnalysisCache.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Interface/ColourIds.h"

namespace audium {

ProjectSerializer::ProjectSerializer(std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
                                     std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                                     std::shared_ptr<PlayListScheduler> playListScheduler_,
                                     std::shared_ptr<LinkAudioDevice> linkAudioDevice_,
                                     std::shared_ptr<AudioBusInterface> audioBusInterface_,
                                     std::shared_ptr<juce::UndoManager> undoManager_,
                                     std::shared_ptr<ProjectFileStore> projectFileStore_) :
    audioTrackContainer(audioTrackContainer_),
    audioResourceContainer(audioResourceContainer_),
    playListScheduler(playListScheduler_),
    linkAudioDevice(linkAudioDevice_),
    audioBusInterface(audioBusInterface_),
    undoManager(undoManager_),
    projectFileStore(projectFileStore_)
{
}

bool ProjectSerializer::writeToJson (json& output)
{
    json jsonAudium;
    audioTrackContainer->writeToJson(jsonAudium);

    jsonAudium["tempo"] = playListScheduler->getTempoProvider()->getTempo();
    jsonAudium["file_version"] = ProjectFileStore::fileVersion;
    jsonAudium["ui_state"] = uiState;
    jsonAudium["scheduler"] = playListScheduler->data;
    output["audium"] = jsonAudium;

    // NOTE: this only serializes; the store's currentJson (the state whose
    // file references are authoritative for obsolete-file cleanup) is updated
    // explicitly by the store's open/save/apply paths, never by snapshots.
    return true;
}

bool ProjectSerializer::readFromJson (json& input, bool rebuild)
{
    auto jsonAudium = input["audium"];

    cleanup(); // clear everything

    const auto tempo = jsonAudium["tempo"].template get<double>();
    if (jsonAudium.contains("file_version"))
    {
        const auto version = jsonAudium["file_version"].template get<int>();
        jassert(version == ProjectFileStore::fileVersion);
    }

    if (jsonAudium.contains("ui_state"))
        uiState = jsonAudium["ui_state"];

    if (jsonAudium.contains("scheduler"))
        playListScheduler->data = jsonAudium["scheduler"];

    if (!linkAudioDevice->getLinkEngine()->isEnabled()) // don't interfere with running sessions
        playListScheduler->getTempoProvider()->setTempo(tempo);

    // Bypass the audio callback for the duration of the destructive rebuild,
    // and never leave it bypassed when the read throws.
    linkAudioDevice->setBypass(true);
    auto readOk = false;
    try {
        readOk = audioTrackContainer->readFromJson(jsonAudium, rebuild);
    }
    catch (...) {
        linkAudioDevice->setBypass(false);
        throw;
    }
    linkAudioDevice->setBypass(false);

    return readOk;
}

bool ProjectSerializer::applyProjectJson (json& input, bool preserveUiState)
{
    // the project structure must never be swapped underneath live recorders -
    // this also covers redo of a reload landing mid-take
    if (audioBusInterface->anyChannelRecording())
        audioBusInterface->record(false);

    auto jsonAudium = input["audium"];

    const auto tempo = jsonAudium["tempo"].template get<double>();
    if (jsonAudium.contains("file_version")) {
        const auto version = jsonAudium["file_version"].template get<int>();
        jassert(version == ProjectFileStore::fileVersion);
    }

    if (!preserveUiState && jsonAudium.contains("ui_state"))
        uiState = jsonAudium["ui_state"];

    if (jsonAudium.contains("scheduler"))
        playListScheduler->data = jsonAudium["scheduler"];

    if (!linkAudioDevice->getLinkEngine()->isEnabled()) // don't interfere with running sessions
        playListScheduler->getTempoProvider()->setTempo(tempo);

    // Non-destructive read; AudioTrackContainer falls back to a full rebuild
    // when the structure differs. Bypass the audio callback for the duration -
    // and make sure a throwing read can't leave it bypassed forever.
    linkAudioDevice->setBypass(true);
    auto readOk = false;
    try {
        readOk = audioTrackContainer->readFromJson(jsonAudium, false);
    }
    catch (...) {
        linkAudioDevice->setBypass(false);
        throw;
    }
    linkAudioDevice->setBypass(false);

    if (!readOk)
        return false;

    playListScheduler->commitPlayListData();

    // The UI/scheduler broadcasts normally happen in
    // AudioTrackContainer::readFromStream; applying JSON directly must
    // publish them here.
    audioTrackContainer->sendActionMessage(rebuildAll);
    audioTrackContainer->sendChangeMessage();

    return true;
}

void ProjectSerializer::cleanup()
{
    audioTrackContainer->cleanup();
    audioResourceContainer->cleanup();
    undoManager->clearUndoHistory();

    uiState.clear();

    if (auto store = projectFileStore.lock())
        store->closeProject();
}

void ProjectSerializer::createNewProject(const int numChannels)
{
    // reset current project dir
    ProjectFileStore::projectDirectory = juce::File();

    // A fresh project starts with no analysis data.
    audioTrackContainer->getAnalysisProvider()->getCache()->clear();

    AudioResourceContainer::createTemporaryProjectDirectory(true);

    audium::WaveFormColours::resetWaveFormColour();
    for (auto i = 0; i < 1; i++) {
        auto track = audioTrackContainer->createNewAudioTrack("Track " + juce::String(i+1));
        track->getViewState().setColour(audioTrackContainer->getNewAudioTrackColour());
        for (auto c = 0; c < numChannels; c++) {
            track->addChannel();
        }
    }
}

} // namespace audium
