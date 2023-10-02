/*
  ==============================================================================

    AudioResourceGroup.h
    Created: 26 Sep 2023 11:22:03am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class AudioResourceContainer;
class AudioResource;

class AudioResourceGroup : public std::enable_shared_from_this<AudioResourceGroup>
{
    
public:
    AudioResourceGroup(const AudioResourceContainer &audioResourceContainer,
                       std::string nameString) :
        owner(audioResourceContainer),
        name(nameString)
    {}
    

    const std::string getName() const { return name; }
    
    const AudioResourceContainer &getAudioResourceContainer() { return owner; }
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResources();
    
    void setColour(juce::Colour colour);
    void updateColour();
    
private:
    const AudioResourceContainer &owner;
    std::string name;
    
};
