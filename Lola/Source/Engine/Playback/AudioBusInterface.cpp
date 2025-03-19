
//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudioBusInterface.h"

namespace audium
{

void AudioBusInterface::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    audioBusRenderer->prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void AudioBusInterface::processAudio(const juce::AudioSourceChannelInfo& outputInfo)
{
    lockFreeCommander->invoke();

    audioBusRenderer->processAudioBlock(outputInfo);
}

void AudioBusInterface::setNumAudioBusChannels(int numChannels)
{
    audioBusRenderer->setNumAudioBusChannels(numChannels);
}

void AudioBusInterface::setPan(const int channelNumber, const float newPan)
{
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr, channelNumber, newPan] {
        ptr->setPan(channelNumber, newPan);
    });
}

void AudioBusInterface::setGain(const int channelNumber, const float newGain)
{
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr, channelNumber, newGain] {
        ptr->setGain(channelNumber, newGain);
    });
}

void AudioBusInterface::setMute(const int channelNumber, const bool bMute)
{
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr, channelNumber, bMute] {
        ptr->setMute(channelNumber, bMute);
    });
}

void AudioBusInterface::setSolo(const int channelNumber, const bool bSolo)
{
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr, channelNumber, bSolo] {
        ptr->setSolo(channelNumber, bSolo);
    });
}

void AudioBusInterface::setMasterGain(const float newGain)
{
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr, newGain] {
        ptr->setMasterGain(newGain);
    });
}


const float AudioBusInterface::getChannelLevel(const int channelNumber) const
{
    return audioBusRenderer->getChannelLevel(channelNumber);
}

const float AudioBusInterface::getMasterLevel(const int channelNumber) const
{
    return audioBusRenderer->getMasterLevel(channelNumber);
}

} // namespace audium
