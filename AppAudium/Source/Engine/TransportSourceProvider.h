/*
  ==============================================================================

    TransportSourceProvider.h
    Created: 14 Jun 2023 6:12:34pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

class AudioResourceContainer;

class TransportSourceProvider
{
    
    
public:
    TransportSourceProvider() = default;
    ~TransportSourceProvider() = default;
    
    std::shared_ptr<juce::AudioTransportSource> createNewTransportSource();
    void removeTransportSource(std::shared_ptr<juce::AudioTransportSource> audioTransportSource);
    
    void setPosition (double newPosition);
    double getCurrentPosition() const;
    

    void start();
    void stop();
    bool isPlaying() const;
    bool playStop();
    
private:
    
    std::vector<std::shared_ptr<juce::AudioTransportSource>> audioTransportSources;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportSourceProvider)
};
