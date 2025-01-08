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

void AudioChannel::setGain(const float new_gain) {
    data.gain = new_gain;
    commitGain();
}

float AudioChannel::getGain() const noexcept {
    return data.gain;
}

void AudioChannel::commitGain()
{
    auto chan = getChannelNumber() + audioTrack.getChannelOffset();
    //std::cout << "setGain " << chan << " " << data.gain << std::endl;
    audioTrack.getTransportSourceContainer()->getPlayback()->setOutputGain(chan, data.gain);
}
