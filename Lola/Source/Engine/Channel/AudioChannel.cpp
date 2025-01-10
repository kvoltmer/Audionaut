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

void AudioChannel::setGain(const float new_gain)
{
    data.gain = new_gain;
    commitChannelData();
}

float AudioChannel::getGain() const noexcept
{
    return data.gain;
}

void AudioChannel::setPan(const float new_pan)
{
    data.pan = new_pan;
    commitChannelData();
}

float AudioChannel::getPan() const noexcept
{
    return data.pan;
}

void AudioChannel::commitChannelData()
{
    auto chan = getChannelNumber() + audioTrack.getChannelOffset();
    // std::cout << "commitChannelData " << chan << " " << data.gain <<  " " << data.pan << std::endl;
    audioTrack.getTransportSourceContainer()->audioBusRenderer->setGain(chan, data.gain);
    audioTrack.getTransportSourceContainer()->audioBusRenderer->setPan(chan, data.pan);
}
