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

// Make sure to define this before <cmath> is included for Windows
#define _USE_MATH_DEFINES
#include <ableton/Link.hpp>
#include <ableton/link/HostTimeFilter.hpp>
#include "LinkEngine.hpp"

namespace audium {

class PlayListScheduler;
class AudioResourceContainer;
class TransportSourceContainer;

class LinkAudioDevice : public juce::AudioIODeviceCallback {
    
public:
    LinkAudioDevice(std::shared_ptr<audium::LinkEngine> linkEngine,
                    std::shared_ptr<PlayListScheduler> playListScheduler);
    ~LinkAudioDevice();
    
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int totalNumInputChannels,
                                           float* const* outputChannelData,
                                           int totalNumOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    
    
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    
    void startPlaying();
    void stopPlaying();
    
    void setBypass(bool isByPass);
    
    audium::LinkEngine* getLinkEngine() const { return linkEngine.get(); }
    
private:
    std::shared_ptr<audium::LinkEngine> linkEngine;
    std::shared_ptr<PlayListScheduler> playListScheduler;
    
    ableton::link::HostTimeFilter<ableton::link::platform::Clock> host_time_filter;
    std::uint64_t sample_time = 0;
    double sampleRate = 0.0;
    int bufferSize = 0;
    std::atomic<bool> byPass;
    
};

} // namespace audium
