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
    
    std::shared_ptr<AudiumTransportSource> createNewTransportSource();
    bool removeTransportSource(std::shared_ptr<AudiumTransportSource> audioTransportSource);
    
    void setLocalPosition (std::shared_ptr<AudioGroup> group, double newPosition);
    double getLocalPosition(std::shared_ptr<AudioGroup> group) const;
    
    void start(std::shared_ptr<AudioGroup> group);
    void stop(std::shared_ptr<AudioGroup> group);
    bool isPlaying(std::shared_ptr<AudioGroup> group) const;
    
private:
    
    std::vector<std::shared_ptr<AudiumTransportSource>> audioTransportSources;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportSourceContainer)
};
