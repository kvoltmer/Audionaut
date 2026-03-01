//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <math.h>
#include <atomic>
#include <iostream>
#include <cassert>

#include "Playback.h"
#include "Engine/AudioSources/AudiumTransportSource.h"

namespace audium
{

Playback::Playback()
{
}

void Playback::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    std::cout << "Playback::prepareToPlay " << samplesPerBlockExpected << " " << sampleRate << std::endl;
}

bool Playback::startVoice(std::shared_ptr<AudiumTransportSource> transportSource)
{
    // voice already playing?
    if (auto voice = findVoice(transportSource))
        if (voice->getTransportSource()->getAudioTransportSource()->isPlaying())
            return true;
    
    // start a new voice
    if (auto newVoice = getAvailableVoice()) {
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

void Playback::stopAllVoices()
{
    // std::cout << "stopAllVoices" << std::endl;
    for(auto i = 0; i < MAX_VOICES; ++i) {
        voices[i].stop();
    }
}

bool Playback::isPlaying(const std::shared_ptr<AudiumTransportSource> source)
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
