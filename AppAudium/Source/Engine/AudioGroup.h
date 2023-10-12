/*
  ==============================================================================

    AudioGroup.h
    Created: 26 Sep 2023 11:22:03am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class AudioResourceContainer;
class AudioResource;
class PlayListContainer;
class PlayListScheduler;

class AudioGroup
{
    
public:
    AudioGroup(const AudioResourceContainer &audioResourceContainer,
               std::shared_ptr<PlayListContainer> playListContainer,
               std::string nameString) :
        audioResourceContainer(audioResourceContainer),
        playListContainer(playListContainer),
        name(nameString)
    {}
    
    ~AudioGroup();
    
    void cleanup();
    

    const std::string getName() const { return name; }
    
    const AudioResourceContainer &getAudioResourceContainer() { return audioResourceContainer; }
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResources();
    
    void setColour(juce::Colour colour);
    void updateColour();
    
    std::shared_ptr<PlayListContainer> getPlayListContainer() const { return playListContainer; }
    
private:
    const AudioResourceContainer &audioResourceContainer;
    std::shared_ptr<PlayListContainer> playListContainer;
    std::string name;
    
};
