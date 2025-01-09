
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
    
private:
    
    Voice* getAvailableVoice();
    
    Voice* findVoice(const std::shared_ptr<AudiumTransportSource> source);
    
    int getNumberOfVoices() const;
    
    juce::AudioBuffer<float> audioBusBuffer;
        
    Voice voices[MAX_VOICES];
    
        
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Playback)
};

} // namespace audium
