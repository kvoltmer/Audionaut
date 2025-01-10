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
    auto c = getChannelNumber() + audioTrack.getChannelOffset();
    
    // commit
    audioTrack.getAudioTrackContainer().lockFreeCommander->fifo.push([this, c] {
        audioTrack.getTransportSourceContainer()->audioBusRenderer->setGain(c, data.gain);
    });
}

float AudioChannel::getGain() const noexcept
{
    return data.gain;
}

void AudioChannel::setPan(const float new_pan)
{
    data.pan = new_pan;
    auto c = getChannelNumber() + audioTrack.getChannelOffset();
    
    // commit
    audioTrack.getAudioTrackContainer().lockFreeCommander->fifo.push([this, c] {
        audioTrack.getTransportSourceContainer()->audioBusRenderer->setPan(c, data.pan);
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
