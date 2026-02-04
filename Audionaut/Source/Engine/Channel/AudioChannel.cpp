//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudioChannel.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/Playback/Playback.h"
#include "Engine/Playback/AudioBusRenderer.h"
#include "Engine/Core/LockFreeCommander.h"

namespace audium {


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
    auto currentDevice = getAudioTrack().getAudioResourceContainer().getAudioDeviceManager()->getCurrentAudioDevice();
    
    if (currentDevice != nullptr &&
        getChannelNumber() < currentDevice->getActiveInputChannels().toInteger()) {
        data.record = bEnabled;
        audioBusInterface->setRecordEnabled(getChannelNumber() + audioTrack.getChannelOffset(),
                                            bEnabled);
    }
    else {
        std::cout << "Error, no input mapped at channel " << getChannelNumber() << std::endl;
    }
}

bool AudioChannel::isRecording() const
{
    return audioBusInterface->isRecording(getChannelNumber() + audioTrack.getChannelOffset());
}

bool AudioChannel::isRecordEnabled() const noexcept
{
    return data.record;
}

void AudioChannel::commitChannelData()
{
    data.channelNumber = getChannelNumber();
    data.trackId = audioTrack.getId();
    audioBusInterface->setChannelData(getChannelNumber() + audioTrack.getChannelOffset(), data);
}

} // namespace audium


