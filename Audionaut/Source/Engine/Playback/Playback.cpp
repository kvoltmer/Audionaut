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

    // the mix buffer is sized here, once; process() then only shrinks the
    // view (avoidReallocating) and never allocates for blocks up to this size
    processingBuffer.setSize(MAX_AUDIO_CHANNELS, samplesPerBlockExpected);
}

bool Playback::startVoice(const std::shared_ptr<VoiceSource>& voiceSource)
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

bool Playback::stopVoice(const std::shared_ptr<VoiceSource>& source,
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
    // std::cout << "stopAllVoices" << std::endl;
    for(auto i = 0; i < MAX_VOICES; ++i) {
        voices[i].stop(true);
    }
}

bool Playback::isPlaying(const std::shared_ptr<VoiceSource>& source)
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

Voice *Playback::findVoice(const std::shared_ptr<VoiceSource>& source)
{
    // pointer compare: no refcount traffic on the audio thread
    for (auto i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].getVoiceSource().get() == source.get())
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
