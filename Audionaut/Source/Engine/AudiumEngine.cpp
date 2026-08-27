//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudiumEngine.h"
#include "Util/Preferences.h"
#include "Engine/ActionMessages.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Analysis/AnalysisCache.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/AudioSources/VoiceSource.h"
#include "Engine/ProjectFileStore.h"
#include "Application/AudiumApplication.h"

#include "Interface/ColourIds.h"

namespace audium {

int AudiumEngine::recordingCounter = 0;

AudiumEngine::~AudiumEngine()
{
    cleanup();
}

void AudiumEngine::initialise()
{
    jassert(RuntimePermissions::isGranted (RuntimePermissions::recordAudio));

    auto numInputChannelsNeeded = MAX_AUDIO_CHANNELS;
    auto numOutputChannelsNeeded = MAX_AUDIO_CHANNELS;
    String result;

#if !defined(AUDIONAUT_HEADLESS)
    if (AudiumApplication::getPreferences().valueExists(PreferenceKeys::audioDeviceSettings)) {
        juce::XmlDocument xml (AudiumApplication::getPreferences().getValue(PreferenceKeys::audioDeviceSettings));
        if (auto saveState = xml.getDocumentElement()) {
            result = audioDeviceManager->initialise(numInputChannelsNeeded,
                                                    numOutputChannelsNeeded,
                                                    saveState.get(),
                                                    true);
        }
    }
    else {
        result = audioDeviceManager->initialiseWithDefaultDevices (numInputChannelsNeeded,
                                                                   numOutputChannelsNeeded);
    }
#else
    result = audioDeviceManager->initialiseWithDefaultDevices (numInputChannelsNeeded,
                                                               numOutputChannelsNeeded);
#endif
    std::cout << result.toStdString() << std::endl;
    audioDeviceManager->addAudioCallback(linkAudioDevice.get());
}

void AudiumEngine::uninitialise()
{
#if !defined(AUDIONAUT_HEADLESS)
    if (auto stateXml = audioDeviceManager->createStateXml()) {
        AudiumApplication::getPreferences().setValue(PreferenceKeys::audioDeviceSettings, stateXml->toString().toStdString());
    }
#endif

    undoManager->clearUndoHistory();
    audioDeviceManager->removeAudioCallback(linkAudioDevice.get());

    // a clean shutdown passed the save/discard prompt - anything left in an
    // autosave would wrongly look like a crash on the next launch
    projectFileStore->deleteAutosave();
}

void AudiumEngine::cleanup()
{
    audioTrackContainer->cleanup();
    audioResourceContainer->cleanup();
    undoManager->clearUndoHistory();

    uiState.clear();
    projectFileStore->closeProject();
}

void AudiumEngine::createNewProject(const int numChannels)
{
    // reset current project dir
    ProjectFileStore::projectDirectory = File();

    // A fresh project starts with no analysis data.
    audioTrackContainer->getAnalysisProvider()->getCache()->clear();

    AudioResourceContainer::createTemporaryProjectDirectory(true);

    audium::WaveFormColours::resetWaveFormColour();
    for (auto i = 0; i < 1; i++) {
        auto track = audioTrackContainer->createNewAudioTrack("Track " + String(i+1));
        track->getViewState().setColour(audioTrackContainer->getNewAudioTrackColour());
        for (auto c = 0; c < numChannels; c++) {
            track->addChannel();
        }
    }
}

void AudiumEngine::setBypass(bool bypass)
{
    linkAudioDevice->setBypass(bypass);
}

bool AudiumEngine::writeToJson (json& output)
{

    json jsonAudium;
    audioTrackContainer->writeToJson(jsonAudium);

    jsonAudium["tempo"] = playListScheduler->getTempoProvider()->getTempo();
    jsonAudium["file_version"] = ProjectFileStore::fileVersion;
    jsonAudium["ui_state"] = uiState;
    jsonAudium["scheduler"] = getPlayListScheduler()->data;
    output["audium"] = jsonAudium;

    // NOTE: this only serializes; the store's currentJson (the state whose
    // file references are authoritative for obsolete-file cleanup) is updated
    // explicitly by the store's open/save/apply paths, never by snapshots.
    // std::cout << std::setw(2) << output << std::endl;
    return true;
}

bool AudiumEngine::readFromJson (json& input, bool rebuild)
{
    // std::cout << std::setw(2) << input << std::endl;
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
        getPlayListScheduler()->data = jsonAudium["scheduler"];


    if (!linkAudioDevice->getLinkEngine()->isEnabled()) // don't interfere with running sessions
        playListScheduler->getTempoProvider()->setTempo(tempo);

    return audioTrackContainer->readFromJson(jsonAudium, rebuild);
}

bool AudiumEngine::applyProjectJson (json& input, bool preserveUiState)
{
    auto jsonAudium = input["audium"];

    const auto tempo = jsonAudium["tempo"].template get<double>();
    if (jsonAudium.contains("file_version")) {
        const auto version = jsonAudium["file_version"].template get<int>();
        jassert(version == ProjectFileStore::fileVersion);
    }

    if (!preserveUiState && jsonAudium.contains("ui_state"))
        uiState = jsonAudium["ui_state"];

    if (jsonAudium.contains("scheduler"))
        getPlayListScheduler()->data = jsonAudium["scheduler"];

    if (!linkAudioDevice->getLinkEngine()->isEnabled()) // don't interfere with running sessions
        playListScheduler->getTempoProvider()->setTempo(tempo);

    // Non-destructive read; AudioTrackContainer falls back to a full rebuild
    // when the structure differs. Bypass the audio callback for the duration -
    // and make sure a throwing read can't leave it bypassed forever.
    setBypass(true);
    auto readOk = false;
    try {
        readOk = audioTrackContainer->readFromJson(jsonAudium, false);
    }
    catch (...) {
        setBypass(false);
        throw;
    }
    setBypass(false);

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

std::shared_ptr<PlayListContainer> AudiumEngine::getPlayListContainer(std::shared_ptr<AudioTrack> track) const
{
    return track->getPlayListContainer();
}

} // namespace audium
