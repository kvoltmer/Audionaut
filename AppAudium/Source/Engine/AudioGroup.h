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
               juce::String nameString,
               int groupId) :
        audioResourceContainer(audioResourceContainer),
        playListContainer(playListContainer),
        transportSourceContainer(transportSourceContainer),
        groupName(nameString),
        groupId(groupId)
    {}
    
    ~AudioGroup();
    
    void cleanup();
    

    const juce::String getName() const { return groupName; }
    const int getId() const noexcept { return groupId; }
    
    void setName(const juce::String newName) { groupName = newName; }
    void setId(const int newId) { groupId = newId; }
    
    const AudioResourceContainer &getAudioResourceContainer() { return audioResourceContainer; }
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesAtChannelPosition(int channelPosition) const;
    
    void setColour(juce::Colour colour);
    juce::Colour getColour() const { return currentColour; }
    
    std::shared_ptr<PlayListContainer> getPlayListContainer() const { return playListContainer; }
    std::shared_ptr<TransportSourceContainer> getTransportSourceContainer() const { return transportSourceContainer; }
    
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);
    
    int getNumChannels() const;
    
private:
    const AudioResourceContainer &audioResourceContainer;
    std::shared_ptr<PlayListContainer> playListContainer;
    std::shared_ptr<TransportSourceContainer> transportSourceContainer;
    juce::String groupName;
    int groupId = -1;
    juce::Colour currentColour = juce::Colours::pink;
};
