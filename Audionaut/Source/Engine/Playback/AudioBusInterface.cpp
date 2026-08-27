
//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudioBusInterface.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Core/HeadlessMode.h"

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
    if (audioBusRenderer->getChannelData(channelNumber).record != data.record) {
        setRecordEnabled(channelNumber, data.record);
    }
    
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr, channelNumber, data] {
        ptr->setChannelData(channelNumber, data);
    });
    
    // With no audio device running there is no render callback to drain the
    // fifo (capacity 256, drops on full) - pump it synchronously. Runtime
    // check so the GUI binary's in-app CLI mode gets the same treatment.
    if (HeadlessMode::isHeadless())
        lockFreeCommander->invoke();
    
}

const AudioChannelData AudioBusInterface::getChannelData(const int channelNumber) const
{
    return audioBusRenderer->getChannelData(channelNumber);
}

void AudioBusInterface::setRecordEnabled(const int channelNumber,
                                         bool bEnabled)
{
    auto recorder = bEnabled ? std::make_shared<AudioRecorder>() : nullptr;
    
    // async!!!
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr, channelNumber, bEnabled, recorder] {
        ptr->setRecordEnabled(channelNumber, bEnabled);
        ptr->getRecording()->setRecordEnabled(channelNumber, bEnabled, recorder);
    });
    
    // in case we record on the fly we have to wait until the recorder exists.
    // see: synchronous call AudioBusInterface::record
    // using a sleepCounter so we don't hang forever :D
    if (bEnabled) {
        auto sleepCounter = 0;
        while (sleepCounter < 10 &&
               audioBusRenderer->getRecording()->getAudioRecorder(channelNumber) == nullptr) {
            juce::Thread::sleep (5);
            sleepCounter++;
        }
    }

}

void AudioBusInterface::record(bool start, const int channelNumber, const double positionClocks)
{
    if (start)
        AudiumEngine::recordingCounter++;
    
    audioBusRenderer->getRecording()->record(start, channelNumber, positionClocks);
}

std::shared_ptr<audium::AudioThumbnail> AudioBusInterface::getRecordingThumbnail(int channelNumber) const
{
    auto recorder = audioBusRenderer->getRecording()->getAudioRecorder(channelNumber);
    if (recorder != nullptr)
        return recorder->getRecordingThumbnail();
    
    return nullptr;
}

const juce::File AudioBusInterface::getRecordedAudioFile(int channelNumber)
{
    auto file = audioBusRenderer->getRecording()->getRecordedFile(channelNumber);
    jassert(file.existsAsFile());
    return file;
}

const double AudioBusInterface::getRecordedLength(int channelNumber) const
{
    auto recorder = audioBusRenderer->getRecording()->getAudioRecorder(channelNumber);
    if (recorder != nullptr)
        return recorder->getTotalLength();
    else
        return 0.0;
}

const double AudioBusInterface::getRecordingStartPosition(int channelNumber) const
{
    return audioBusRenderer->getRecording()->getRecordingStartPosition(channelNumber);
}

bool AudioBusInterface::isRecording(int channelNumber) const
{
    auto recorder = audioBusRenderer->getRecording()->getAudioRecorder(channelNumber);
    if (recorder != nullptr)
        return recorder->isRecording();
    else
        return false;
}

bool AudioBusInterface::anyChannelRecording() const
{
    for (auto i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
        if (auto recorder = audioBusRenderer->getRecording()->getAudioRecorder(i)) {
            if (recorder->isRecording())
                return true;
        }
    }
    return false;
}

void AudioBusInterface::setMasterGain(const float newGain)
{
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr, newGain] {
        ptr->setMasterGain(newGain);
    });
}

void AudioBusInterface::resetGains()
{
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr] {
        ptr->resetGains();
    });
}

void AudioBusInterface::setStemExport(bool bStemExport)
{
    // direct call, not via fifo: the device callback is bypassed during export,
    // so nothing would drain the fifo (see AudioExporter)
    audioBusRenderer->setStemExport(bStemExport);
}

void AudioBusInterface::stopAllVoices()
{
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr] {
        ptr->getPlayback()->stopAllVoices();
    });
}

const float AudioBusInterface::getChannelLevel(const int channelNumber) const
{
    return audioBusRenderer->getChannelLevel(channelNumber);
}

const float AudioBusInterface::getRecordingLevel(const int channelNumber) const
{
    return audioBusRenderer->getRecordingLevel(channelNumber);
}

const float AudioBusInterface::getMasterLevel(const int channelNumber) const
{
    return audioBusRenderer->getMasterLevel(channelNumber);
}

} // namespace audium
