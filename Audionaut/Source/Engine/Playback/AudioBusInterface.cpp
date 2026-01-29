
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
                                         bool bEnabled)
{
    auto recorder = bEnabled ? std::make_shared<AudioRecorder>() : nullptr;
    
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr, channelNumber, bEnabled, recorder] {
        ptr->setRecordEnabled(channelNumber, bEnabled, recorder);
    });
}

void AudioBusInterface::setRecordingThumbnail(AudioThumbnail *audioThumbnail, int channelNumber)
{
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr, channelNumber, audioThumbnail] {
        ptr->setRecordingThumbnail(audioThumbnail, channelNumber);
    });
    
}

void AudioBusInterface::record(bool start)
{
    audioBusRenderer->record(start);
}

const juce::File AudioBusInterface::getRecordedAudioFile(int channelNumber)
{
    return audioBusRenderer->getAudioRecorder(channelNumber)->getRecordedFile();
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


void AudioBusInterface::reset()
{
    // delete all recorders
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr] {
        ptr->clearRecorders();
    });
}

} // namespace audium
