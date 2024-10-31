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

class TransportSourceContainer
{
public:
    TransportSourceContainer() = default;
    ~TransportSourceContainer() = default;
    
    std::shared_ptr<AudiumTransportSource> createAndAddTransportSource(AudioResource& audioResource,
                                                                       std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource);
    bool removeTransportSource(std::shared_ptr<AudiumTransportSource> audioTransportSource);
    
    void cleanup();
    
    void prepareToPlay (double sampleRate, int blockSize);
    
    void startPlaying();
    void stopPlaying();
    bool isPlaying() const;
    
    void audioCallback(const juce::AudioSourceChannelInfo& info);
    void setBypass(bool isByPass) { byPass = isByPass; }

    std::shared_ptr<AudiumTransportSource> getTransportSourceAtIndex(int index) const;
    int getTransportSourceIndex(std::shared_ptr<AudiumTransportSource> searchTransportSource) const;
    
    const float getOutputLevel(const int trackNumber, const int channelNumber) const;
    
private:
    std::atomic<bool> playing = false;
    std::atomic<bool> byPass = false;
    juce::CriticalSection callbackLock;
    juce::Array<std::shared_ptr<AudiumTransportSource>> audioTransportSources;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportSourceContainer)
};
