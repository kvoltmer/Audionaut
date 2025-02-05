/*
  ==============================================================================

    AudioChannel.cpp
    Created: 1 Jan 2025 5:56:25pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioChannel.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/Playback/Playback.h"
#include "Engine/Playback/AudioBusRenderer.h"
#include "Engine/Core/LockFreeCommander.h"

void AudioChannel::setGain(const float newGain)
{
    data.gain = newGain;
    audioBusInterface->setGain(getChannelNumber() + audioTrack.getChannelOffset(),
                               newGain);
}

float AudioChannel::getGain() const noexcept
{
    return data.gain;
}

void AudioChannel::setPan(const float newPan)
{
    data.pan = newPan;
    audioBusInterface->setPan(getChannelNumber() + audioTrack.getChannelOffset(),
                              newPan);
}

float AudioChannel::getPan() const noexcept
{
    return data.pan;
}

void AudioChannel::setMute(bool bMute)
{
    data.mute = bMute;
    audioBusInterface->setMute(getChannelNumber() + audioTrack.getChannelOffset(),
                               bMute);
}
bool AudioChannel::getMute() const noexcept
{
    return data.mute;
}

void AudioChannel::setSolo(bool bSolo)
{
    data.solo = bSolo;
    audioBusInterface->setSolo(getChannelNumber() + audioTrack.getChannelOffset(),
                               bSolo);
}

bool AudioChannel::getSolo() const noexcept
{
    return data.solo;
}

void AudioChannel::commitChannelData()
{
    setGain(data.gain);
    setPan(data.pan);
    setMute(data.mute);
    setSolo(data.solo);
}
