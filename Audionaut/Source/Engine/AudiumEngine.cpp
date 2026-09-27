//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudiumEngine.h"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Project/ProjectSerializer.h"

namespace audium {

AudiumEngine::~AudiumEngine()
{
    projectSerializer->cleanup();
}

void AudiumEngine::initialise(const juce::XmlElement* savedAudioDeviceState)
{
    jassert(RuntimePermissions::isGranted (RuntimePermissions::recordAudio));

    auto numInputChannelsNeeded = MAX_AUDIO_CHANNELS;
    auto numOutputChannelsNeeded = MAX_AUDIO_CHANNELS;
    String result;

    if (savedAudioDeviceState != nullptr) {
        result = audioDeviceManager->initialise(numInputChannelsNeeded,
                                                numOutputChannelsNeeded,
                                                savedAudioDeviceState,
                                                true);
    }
    else {
        result = audioDeviceManager->initialiseWithDefaultDevices (numInputChannelsNeeded,
                                                                   numOutputChannelsNeeded);
    }
    std::cout << result.toStdString() << std::endl;
    audioDeviceManager->addAudioCallback(linkAudioDevice.get());
}

void AudiumEngine::uninitialise()
{
    undoManager->clearUndoHistory();
    audioDeviceManager->removeAudioCallback(linkAudioDevice.get());

    // a clean shutdown passed the save/discard prompt - anything left in an
    // autosave would wrongly look like a crash on the next launch
    projectFileStore->deleteAutosave();
}

} // namespace audium
