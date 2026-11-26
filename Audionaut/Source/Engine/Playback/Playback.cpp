//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <math.h>
#include <atomic>
#include <iostream>
#include <cassert>

#include "Playback.h"
#include "Engine/AudioSources/VoiceSource.h"

namespace audium
{

Playback::Playback()
{
}

void Playback::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    DBG("Playback::prepareToPlay " << samplesPerBlockExpected << " " << sampleRate);
    // the output buffer a voice renders into belongs to the caller (AudioBusRenderer)
    juce::ignoreUnused (samplesPerBlockExpected, sampleRate);
}

int Playback::getActiveVoiceChannel (int voiceIndex) const noexcept
{
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES || ! voices[voiceIndex].processing.load())
        return -1;

    if (auto* source = voices[voiceIndex].getVoiceSource())
        return source->getDestinationChannel();

    return -1;
}

void Playback::stopVoiceNow (int voiceIndex) noexcept
{
    if (voiceIndex >= 0 && voiceIndex < MAX_VOICES)
        voices[voiceIndex].stopNow();
}

bool Playback::startVoice(VoiceSource* voiceSource)
{
    // voice already playing?
    if (auto voice = findVoice(voiceSource))
        if (voice->getVoiceSource()->isPlaying())
            return true;
    
    // start a new voice
    if (auto newVoice = getAvailableVoice()) {
        newVoice->start(voiceSource);
        return true;
    }
    return false;
}

bool Playback::stopVoice(VoiceSource* source,
                         bool fadeOutLastBlock)
{
    // Voice::stop is a no-op for a source that is not playing; skip the
    // voice scan for it
    if (source == nullptr || ! source->isPlaying())
        return false;

    if (auto voice = findVoice(source)) {
        voice->stop(fadeOutLastBlock);
        return true;
    }
    return false;
}

void Playback::stopAllVoices()
{
    for(auto i = 0; i < MAX_VOICES; ++i) {
        voices[i].stop(true);
    }
}

bool Playback::isPlaying(VoiceSource* source)
{
    if (findVoice(source) != nullptr) {
        return true;
    }
    return false;
}

int Playback::getNumVoices() const
{
    int counter = 0;
    for (auto i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].processing.load())
            counter++;
    }
    return counter;
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

Voice *Playback::findVoice(const VoiceSource* source)
{
    for (auto i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].getVoiceSource() == source)
            return &voices[i];
    }
    return nullptr;
}

bool Playback::isVoiceSourceActive(const VoiceSource* source) const noexcept
{
    for (auto i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].getVoiceSource() == source)
            return true;
    }
    return false;
}


} // namespace audium
