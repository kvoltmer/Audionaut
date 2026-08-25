//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudioChannel.h"
#include "Engine/AudioSources/VoiceSourceContainer.h"
#include "Engine/Playback/Playback.h"
#include "Engine/Playback/AudioBusRenderer.h"
#include "Engine/Core/LockFreeCommander.h"

namespace audium {

int AudioChannel::getChannelHeight() const noexcept {
    if (audioTrack.getViewState().getMinimized())
        return 0;
    
    return data.height;
}

void AudioChannel::setGain(const float newGain)
{
    data.gain = newGain;
    commitChannelData();
}

float AudioChannel::getGain() const noexcept
{
    return data.gain;
}

void AudioChannel::setPan(const float newPan)
{
    data.pan = newPan;
    commitChannelData();
}

float AudioChannel::getPan() const noexcept
{
    return data.pan;
}

void AudioChannel::setMute(bool bMute)
{
    data.mute = bMute;
    commitChannelData();
}
bool AudioChannel::getMute() const noexcept
{
    return data.mute;
}

void AudioChannel::setSolo(bool bSolo)
{
    data.solo = bSolo;
    commitChannelData();
}

bool AudioChannel::getSolo() const noexcept
{
    return data.solo;
}

void AudioChannel::setRecordEnabled(bool bEnabled)
{
    auto numInputChannels = 0;
    
#if AUDIONAUT_HEADLESS
    numInputChannels = 1;
#else
    auto currentDevice = getAudioTrack().getAudioResourceContainer().getAudioDeviceManager()->getCurrentAudioDevice();
    
    if (currentDevice != nullptr) {
        numInputChannels = currentDevice->getActiveInputChannels().toInteger();
    }
#endif
    
    if (getChannelNumber() < numInputChannels) {
        data.record = bEnabled;
        audioBusInterface->setRecordEnabled(getChannelNumber() + audioTrack.getChannelOffset(), bEnabled);
    }
    else {
        data.record = false;
        std::cout << "Error, no input mapped at channel " << getChannelNumber() << std::endl;
    }
    
    commitChannelData();
}

bool AudioChannel::isRecording() const
{
    return audioBusInterface->isRecording(getChannelNumber() + audioTrack.getChannelOffset());
}

bool AudioChannel::isRecordEnabled() const noexcept
{
    jassert(data.record == audioBusInterface->getChannelData(getChannelNumber() + audioTrack.getChannelOffset()).record);
    return data.record;
}

void AudioChannel::commitChannelData()
{
    data.channelNumber = getChannelNumber();
    data.trackId = audioTrack.getId();
    audioBusInterface->setChannelData(getChannelNumber() + audioTrack.getChannelOffset(), data);
}

} // namespace audium


