
#include <math.h>
#include <atomic>
#include <iostream>
#include <cassert>

#include "Playback.h"
#include "Engine/AudioSources/AudiumTransportSource.h"

namespace audium
{

/// Start the processing
void Playback::start()
{
    readyToProcess.store(true);
}

/// Stop the processing
void Playback::stop()
{
    readyToProcess.store(false);
    for(auto i = 0; i < MAX_VOICES; ++i) {
        voices[i].stop();
    }
}

bool Playback::startVoice(std::shared_ptr<AudiumTransportSource> transportSource)
{
    if(auto newVoice = getAvailableVoice()) {
        newVoice->start(transportSource);
        return true;
    }
    return false;
}

bool Playback::stopVoice(const std::shared_ptr<AudiumTransportSource> source)
{
    if (auto voice = findVoice(source)) {
        voice->stop();
        return true;
    }
    return false;
}

bool Playback::isPlaying(const std::shared_ptr<AudiumTransportSource> source)
{
    if (findVoice(source) != nullptr) {
        return true;
    }
    return false;
}

void Playback::processAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    if (readyToProcess.load())
    {
        // avoid reallocating
        audioBusBuffer.setSize(info.buffer->getNumChannels(), info.numSamples, false, false, true);
        audioBusBuffer.clear();
        juce::AudioSourceChannelInfo audioBusInfo (&audioBusBuffer, info.startSample, info.numSamples);
        
        for (auto i = 0; i < MAX_VOICES; ++i) {
            if (voices[i].processing.load()) {
                voices[i].processAudioBlock(audioBusInfo);
                for (auto c = 0; c < info.buffer->getNumChannels(); c++) {
                    info.buffer->addFrom(c, info.startSample, audioBusBuffer.getReadPointer(c), info.numSamples);
                }
            }
        }

        
        for (auto i = 0; i < std::min(info.buffer->getNumChannels(), MAX_AUDIO_CHANNELS); i++) {
            outputLevel[i] = info.buffer->getMagnitude(i, info.startSample, info.numSamples);
        }
    }
    else {
        std::memset(outputLevel, 0, sizeof(float) * MAX_AUDIO_CHANNELS);
    }
    
    
}


const float Playback::getOutputLevel(const int channelNumber) const
{
    if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS)
        return outputLevel[channelNumber];
    
    return 0.f;
}

Voice *Playback::getAvailableVoice()
{
    for (auto i = 0; i < MAX_VOICES; ++i) {
        if (!voices[i].processing.load())
            return &voices[i];
    }
    jassertfalse;
    return nullptr;
}

Voice *Playback::findVoice(const std::shared_ptr<AudiumTransportSource> source)
{
    for (auto i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].getTransportSource() == source)
            return &voices[i];
    }
    return nullptr;
}

int Playback::getNumberOfVoices() const
{
    auto counter = 0;
    for (auto i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].processing.load())
            counter++;
    }
    return counter;
}

} // namespace audium
