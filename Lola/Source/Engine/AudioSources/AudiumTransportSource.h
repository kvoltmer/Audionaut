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

#include "Engine/PlayList/SampleTimer.h"
#include "Engine/AudioSources/audium_AudioTransportSource.h"

namespace audium {

#define MAX_AUDIO_FILE_CHANNELS 64

class AudioTrack;
class AudioResource;

class AudiumTransportSource : public juce::AudioSource
{
public:
    AudiumTransportSource(AudioResource& audioResource,
                          std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource);
    
    ~AudiumTransportSource() override
    {
        audioTransportSource->setSource(nullptr);
    }
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    
    void releaseResources() override
    {
        if (mainSource != nullptr)
            mainSource->releaseResources();
    }
    
    void schedulePosition (double newPosition, int startSample)
    {
        if (startSample == 0)
        {
            audioTransportSource->setPosition(newPosition);
        }
        else
        {
            scheduledStartSample.store(startSample);
            scheduledPosition = newPosition;
        }
    }
    
    void scheduleDuration(double duration, double sr)
    {
        durationTimer.schedule(static_cast<int>(duration * sr));
    }
    
    bool isPlaying() const noexcept
    {
        return audioTransportSource->isPlaying();
    }
    
    bool isStopped() const noexcept
    {
        return audioTransportSource->isStopped();
    }
    
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override;
    
    
    AudioResource& getAudioResource() const { return audioResource; }
    
    void applyChannelMapping();
    
    std::shared_ptr<audium::AudioTransportSource> getAudioTransportSource() const { return audioTransportSource; }
    
#if CATCH2_TESTS
    int64 samplesProcessed = 0;
#endif
    
private:
    
    
    AudioResource& audioResource;
    
    // the sample position where the position change should happen
    std::atomic<int> scheduledStartSample = 0;
    // the scheduled position change
    std::atomic<double> scheduledPosition = 0.0;
    
    audium::SampleTimer durationTimer;
    
    std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource;
    
    std::shared_ptr<audium::AudioTransportSource> audioTransportSource;
    
    std::unique_ptr<juce::ChannelRemappingAudioSource> channelRemapping;
    
    juce::AudioSource* mainSource = nullptr;
    
};

} // namespace audium 
