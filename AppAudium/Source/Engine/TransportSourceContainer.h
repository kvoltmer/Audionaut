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
class AudioGroup;
class AudiumTransportSource;

class TransportSourceContainer
{
public:
    TransportSourceContainer() = default;
    ~TransportSourceContainer() = default;
    
    void cleanup() { audioTransportSources.clear(); }
    
    std::shared_ptr<AudiumTransportSource> createNewTransportSource();
    bool removeTransportSource(std::shared_ptr<AudiumTransportSource> audioTransportSource);
    
    void setLocalPosition (double newPosition);
    double getLocalPosition() const;
    
    void startPlaying();
    void stopPlaying();
    bool isPlaying() const;
    
private:
    
    std::vector<std::shared_ptr<AudiumTransportSource>> audioTransportSources;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportSourceContainer)
};
