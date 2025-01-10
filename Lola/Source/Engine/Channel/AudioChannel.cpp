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

void AudioChannel::setGain(const float new_gain)
{
    data.gain = new_gain;
    
    // commit
    auto lockFreeCommander = audioTrack.getAudioTrackContainer().lockFreeCommander;
    lockFreeCommander->fifo.push([this] {
        auto chan = getChannelNumber() + audioTrack.getChannelOffset();
        audioTrack.getTransportSourceContainer()->audioBusRenderer->setGain(chan, data.gain);
    });
}

float AudioChannel::getGain() const noexcept
{
    return data.gain;
}

void AudioChannel::setPan(const float new_pan)
{
    data.pan = new_pan;
    
    // commit
    auto lockFreeCommander = audioTrack.getAudioTrackContainer().lockFreeCommander;
    lockFreeCommander->fifo.push([this] {
        auto chan = getChannelNumber() + audioTrack.getChannelOffset();
        audioTrack.getTransportSourceContainer()->audioBusRenderer->setPan(chan, data.pan);
    });
}

float AudioChannel::getPan() const noexcept
{
    return data.pan;
}

void AudioChannel::commitChannelData()
{
    setGain(data.gain);
    setPan(data.pan);
}
