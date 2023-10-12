/*
  ==============================================================================

    AudiumTransportSource.h
    Created: 6 Oct 2023 11:38:55am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

class AudioGroup;

class AudiumTransportSource : public juce::AudioTransportSource
{
public:
    AudiumTransportSource() = default;
    
    void setAudioGroup(std::shared_ptr<AudioGroup> group) { audioGroup = group; }
    
    std::shared_ptr<AudioGroup> getAudioGroup() const { return audioGroup; }
    
private:
    std::shared_ptr<AudioGroup> audioGroup;
};
