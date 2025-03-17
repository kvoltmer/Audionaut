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

#pragma once

#include <JuceHeader.h>

#include "Voice.h"
#include "PlaybackDefines.h"

namespace audium
{

/// Forward declarations
class Playback;

class Playback
{
public:
    
    Playback();
    ~Playback() = default;
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);
    
    bool startVoice(std::shared_ptr<AudiumTransportSource> source);
    bool stopVoice(const std::shared_ptr<AudiumTransportSource> source);
    
    void stopAllVoices();
    
    bool isPlaying(const std::shared_ptr<AudiumTransportSource> source);
    
    void processAudioBlock (const juce::AudioSourceChannelInfo& info);
    
    int getNumVoices() const;
    
private:
    
    Voice* getAvailableVoice();
    
    Voice* findVoice(const std::shared_ptr<AudiumTransportSource> source);
    
    int getNumberOfVoices() const;
    
    juce::AudioBuffer<float> audioBusBuffer;
        
    Voice voices[MAX_VOICES];
    
        
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Playback)
};

} // namespace audium
