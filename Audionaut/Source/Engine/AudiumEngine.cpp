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

    // Without a requested rate JUCE opens the device at the available rate
    // nearest to zero, 8 kHz on ALSA's default device. Ask for the default
    // rate instead; a saved set-up that names its own rate still wins, and a
    // device without this rate gets the nearest one it has.
    juce::AudioDeviceManager::AudioDeviceSetup preferredSetup;
    preferredSetup.sampleRate = defaultSampleRate;

    const auto result = audioDeviceManager->initialise(numInputChannelsNeeded,
                                                       numOutputChannelsNeeded,
                                                       savedAudioDeviceState,
                                                       true,
                                                       {},
                                                       &preferredSetup);
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
