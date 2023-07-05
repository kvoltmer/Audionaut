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

class TransportSourceContainer
{
    
    
public:
    TransportSourceContainer() = default;
    ~TransportSourceContainer() = default;
    
    std::shared_ptr<juce::AudioTransportSource> createNewTransportSource();
    
    void setPosition (double newPosition);
    double getCurrentPosition() const;
    

    void start();
    void stop();
    bool isPlaying() const;
    
private:
    
    std::vector<std::shared_ptr<juce::AudioTransportSource>> audioTransportSources;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportSourceContainer)
};
