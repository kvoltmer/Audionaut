/*
  ==============================================================================

    AudioGroup.h
    Created: 26 Sep 2023 11:22:03am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "AudioRegion.h"

class AudioResourceContainer;
class AudioResource;
class PlayListContainer;
class PlayListScheduler;
class TransportSourceContainer;


class AudioGroup
{
    
public:
    AudioGroup(const AudioResourceContainer &audioResourceContainer,
               std::shared_ptr<PlayListContainer> playListContainer,
               std::shared_ptr<TransportSourceContainer> transportSourceContainer,
               std::string nameString,
               int groupId) :
        audioResourceContainer(audioResourceContainer),
        playListContainer(playListContainer),
        transportSourceContainer(transportSourceContainer),
        name(nameString),
        groupId(groupId)
    {}
    
    ~AudioGroup();
    
    void cleanup();
    

    const std::string getName() const { return name; }
    const int getId() const noexcept { return groupId; }
    
    const AudioResourceContainer &getAudioResourceContainer() { return audioResourceContainer; }
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResources();
    
    void setColour(juce::Colour colour);
    void updateColour();
    
    std::shared_ptr<PlayListContainer> getPlayListContainer() const { return playListContainer; }
    std::shared_ptr<TransportSourceContainer> getTransportSourceContainer() const { return transportSourceContainer; }
        
private:
    const AudioResourceContainer &audioResourceContainer;
    std::shared_ptr<PlayListContainer> playListContainer;
    std::shared_ptr<TransportSourceContainer> transportSourceContainer;
    std::string name;
    int groupId;
    
};
