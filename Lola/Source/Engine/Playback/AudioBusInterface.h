#pragma once

#include <JuceHeader.h>

#include "Engine/Playback/AudioBusRenderer.h"
#include "Engine/Core/LockFreeCommander.h"

namespace audium
{

// thread save audio bus interface
class AudioBusInterface
{
    
public:
    
    AudioBusInterface(std::shared_ptr<audium::LockFreeCommander> lockFreeCommander_,
                      std::shared_ptr<audium::AudioBusRenderer<float>> audioBusRenderer_) :
        lockFreeCommander(lockFreeCommander_),
        audioBusRenderer(audioBusRenderer_)
    {
    }
    
    ~AudioBusInterface() = default;
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);
    
    void processAudio(const juce::AudioSourceChannelInfo& outputInfo);
    
    void setNumAudioBusChannels(int numChannels);
    
    void setPan(const int channelNumber, const float newPan);
    void setGain(const int channelNumber, const float newGain);
    void setMute(const int channelNumber, const bool bMute);
    void setSolo(const int channelNumber, const bool bSolo);
    
    void setMasterGain(const float newGain);
    
    const float getChannelLevel(const int channelNumber) const;
    const float getMasterLevel(const int channelNumber) const;

private:


    std::shared_ptr<audium::LockFreeCommander> lockFreeCommander;
    std::shared_ptr<audium::AudioBusRenderer<float>> audioBusRenderer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioBusInterface)

};

} // namespace audium
