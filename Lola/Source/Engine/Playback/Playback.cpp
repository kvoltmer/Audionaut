//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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

void Playback::processAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    // avoid reallocating
    auto numChannels = std::min(info.buffer->getNumChannels(), MAX_AUDIO_CHANNELS);
    audioBusBuffer.setSize(numChannels, info.numSamples, false, false, true);
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
