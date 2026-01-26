
//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudioBusInterface.h"

namespace audium
{

void AudioBusInterface::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    audioBusRenderer->prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void AudioBusInterface::setNumAudioBusChannels(int numChannels)
{
    audioBusRenderer->setNumAudioBusChannels(numChannels);
}

void AudioBusInterface::setChannelData(const int channelNumber, const AudioChannelData data)
{
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr, channelNumber, data] {
        ptr->setChannelData(channelNumber, data);
    });
}

void AudioBusInterface::setRecordEnabled(const int channelNumber,
                                         bool bEnabled,
                                         std::shared_ptr<juce::AudioThumbnail> thumbnail)
{
    auto recorder = bEnabled ? std::make_shared<AudioRecorder>(thumbnail) : nullptr;
    
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr, channelNumber, bEnabled, recorder] {
        ptr->setRecordEnabled(channelNumber, bEnabled, recorder);
    });
}

void AudioBusInterface::record(bool start)
{
    audioBusRenderer->record(start);
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
