//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudiumEngine.h"
#include "Util/Preferences.h"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Application/AudiumApplication.h"

namespace audium {

AudiumEngine::~AudiumEngine()
{
    projectSerializer->cleanup();
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

void AudiumEngine::setBypass(bool bypass)
{
    linkAudioDevice->setBypass(bypass);
}

} // namespace audium
