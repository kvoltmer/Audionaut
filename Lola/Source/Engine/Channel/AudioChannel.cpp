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
    auto c = getChannelNumber() + audioTrack.getChannelOffset();
    audioBusInterface->setGain(c, newGain);
}

float AudioChannel::getGain() const noexcept
{
    return data.gain;
}

void AudioChannel::setPan(const float newPan)
{
    data.pan = newPan;
    auto c = getChannelNumber() + audioTrack.getChannelOffset();
    audioBusInterface->setPan(c, newPan);
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

const float AudioChannel::getOutputLevel() const
{
    if (audioBusInterface) {
        auto channel = getAudioTrack().getChannelOffset() + getChannelNumber();
        return audioBusInterface->getOutputLevel(channel);
    }

    return 0.f;
}
