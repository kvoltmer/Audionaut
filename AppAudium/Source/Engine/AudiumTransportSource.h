/*
  ==============================================================================

    AudiumTransportSource.h
    Created: 6 Oct 2023 11:38:55am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

class AudioResourceGroup;

class AudiumTransportSource : public juce::AudioTransportSource
{
public:
    AudiumTransportSource() = default;
    
    void setAudioResourceGroup(std::shared_ptr<AudioResourceGroup> group) { audioResourceGroup = group; }
    
private:
    std::shared_ptr<AudioResourceGroup> audioResourceGroup;
};
