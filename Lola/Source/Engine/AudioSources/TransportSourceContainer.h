/*
  ==============================================================================

    TransportSourceContainer.h
    Created: 14 Jun 2023 6:12:34pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

class AudioResourceContainer;
class AudioTrack;
class AudiumTransportSource;
class AudioResource;

namespace audium {
    class Playback;
}

class TransportSourceContainer
{
public:
    TransportSourceContainer(std::shared_ptr<audium::Playback> playback_) :
        playback(playback_)
    {}
    ~TransportSourceContainer() = default;
    
    void prepareToPlay (int samplesPerBlockExpected,
                        double sampleRate);
    void cleanup();

    std::shared_ptr<AudiumTransportSource> createAndAddTransportSource(AudioResource& audioResource,
                                                                       std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource);
    bool removeTransportSource(std::shared_ptr<AudiumTransportSource> audioTransportSource);
    
    std::shared_ptr<AudiumTransportSource> getTransportSourceAtIndex(int index) const;
    int getTransportSourceIndex(std::shared_ptr<AudiumTransportSource> searchTransportSource) const;
    
    const float getOutputLevel(const int channelNumber) const;
     
    void applyChannelMapping();
    
private:
    
    std::vector<std::shared_ptr<AudiumTransportSource>> audioTransportSources;
    
    std::shared_ptr<audium::Playback> playback;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportSourceContainer)
};
