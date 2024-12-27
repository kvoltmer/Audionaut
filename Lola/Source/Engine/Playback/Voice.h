#pragma once

#include <JuceHeader.h>

class AudiumTransportSource;

namespace audium
{

class Voice
{
    
public:
    
    Voice() = default;
    ~Voice() = default;
    
    void processAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill);
    
    void start(std::shared_ptr<AudiumTransportSource> transportSource);
    void stop();
    
    std::atomic<bool> processing;
    
    const std::shared_ptr<AudiumTransportSource> getTransportSource() const { return transportSource; }
    
private:

    std::shared_ptr<AudiumTransportSource> transportSource;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Voice)

};

} // namespace audium
