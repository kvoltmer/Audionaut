
#pragma once

#include <JuceHeader.h>
#include "PlaybackDefines.h"

class AudioTrackContainer;

namespace audium
{

class Playback;

template <class SampleType>
class AudioBusRenderer {
    
    
public:
    AudioBusRenderer(std::shared_ptr<audium::Playback> playback_) :
        playback(playback_)
    {
    }
    
    ~AudioBusRenderer() = default;
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);
    
    void setNumAudioBusChannels(int numChannels);
    
    void processAudioBlock(const juce::AudioSourceChannelInfo& outputInfo);
    
    void setPan(const int channelNumber, const SampleType newPan) {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
            panners[channelNumber].setPan(newPan);
        }
    }

    void setGain(const int channelNumber, const SampleType newGain) {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
            gains[channelNumber].setGainLinear(newGain);
        }
    }
    
    const float getOutputLevel(const int channelNumber) const
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS)
            return outputLevel[channelNumber];
        
        return 0.f;
    }
    
private:
    
    std::shared_ptr<audium::Playback> playback;
    
    juce::AudioBuffer<SampleType> audioBus;
    juce::AudioBuffer<SampleType> channelBuffer;
    juce::AudioBuffer<SampleType> stereoBuffer;
        
    juce::dsp::Panner<SampleType> panners[MAX_AUDIO_CHANNELS];
    juce::dsp::Gain<SampleType> gains[MAX_AUDIO_CHANNELS];
    
    std::atomic<float> outputLevel[MAX_AUDIO_CHANNELS];
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioBusRenderer)

};

} // namespace audium
