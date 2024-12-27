
#pragma once

#include <JuceHeader.h>

#include "Voice.h"

namespace audium
{

/// Forward declarations
class Playback;

/// Defines
#define MAX_VOICES 64
#define MAX_AUDIO_CHANNELS 64


class Playback
{
public:
    
    Playback() = default;
    ~Playback() = default;
    
    void start();
    void stop();
    
    bool startVoice(std::shared_ptr<AudiumTransportSource> source);
    bool stopVoice(const std::shared_ptr<AudiumTransportSource> source);
    
    void processAudioBlock (const juce::AudioSourceChannelInfo& info);
    
    const float getOutputLevel(const int channelNumber) const;
    
private:
    
    Voice* getAvailableVoice();
    
    Voice* findVoice(const std::shared_ptr<AudiumTransportSource> source);
    
    int getNumberOfVoices() const;
    
    juce::AudioBuffer<float> audioBusBuffer;
    
    std::atomic<bool> readyToProcess;
    
    Voice voices[MAX_VOICES];
    
    std::atomic<float> outputLevel[MAX_AUDIO_CHANNELS];
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Playback)
};

} // namespace audium
