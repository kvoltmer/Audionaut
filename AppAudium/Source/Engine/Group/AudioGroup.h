/*
  ==============================================================================

    AudioGroup.h
    Created: 26 Sep 2023 11:22:03am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/Streamable.h"

class AudioResourceContainer;
class AudioResource;
class PlayListContainer;
class PlayListScheduler;
class TransportSourceContainer;
class AudioSubGroup;
class AudioRegionContainer;
class AudioChannel;
class PositionableBase;
class AudioGroupContainer;

class AudioGroup : public audium::Streamable
{
    
public:
    AudioGroup(AudioGroupContainer &owner,
               AudioResourceContainer &audioResourceContainer,
               AudioRegionContainer &audioRegionContainer,
               std::shared_ptr<PlayListContainer> playListContainer,
               std::shared_ptr<TransportSourceContainer> transportSourceContainer,
               juce::String nameString,
               int groupId) :
        owner(owner),
        audioResourceContainer(audioResourceContainer),
        audioRegionContainer(audioRegionContainer),
        playListContainer(playListContainer),
        transportSourceContainer(transportSourceContainer),
        groupName(nameString),
        groupId(groupId)
    {}
    
    ~AudioGroup();
    
    void cleanup();
    

    const juce::String getName() const { return groupName; }
    void setName(const juce::String newName) { groupName = newName; }
    
    const int getId() const noexcept { return groupId; }
    void setId(const int newId) { groupId = newId; }
    
    AudioResourceContainer &getAudioResourceContainer() const { return audioResourceContainer; }
    AudioGroupContainer &getAudioGroupContainer() const { return owner; }
    AudioRegionContainer &getAudioRegionContainer() const { return audioRegionContainer; }
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesAtChannelPosition(int channelPosition) const;
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesAtAbsoluteRange(juce::Range<double> rangeInSeconds) const;
    
    void setColour(juce::Colour colour);
    juce::Colour getColour() const { return currentColour; }
    
    std::shared_ptr<PlayListContainer> getPlayListContainer() const { return playListContainer; }
    std::shared_ptr<TransportSourceContainer> getTransportSourceContainer() const { return transportSourceContainer; }
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream) override;
    int getSizeInUnits() override;
    
    int getNumChannels() const;
    void ensureNumChannels(int channelsNeeded);
    std::shared_ptr<AudioChannel> getChannel(int channelNumber) const { return audioChannels[channelNumber]; }
    int getChannelNumberFor(AudioChannel* channel);
    
    int getTotalHeight() const;
    float getOutputLevel(int channelNumber) const;
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesAtChannel(int channelNumber) const;
    
    void setGain(float gain, int channelNumber);
    float getGain(int channelNumber) const;
    
    int getNextSubGroupId() { return ++nextSubGroupId; }
    std::shared_ptr<AudioSubGroup> createNewAudioSubGroup(double transportPosition = 0.0, int subGroupId = -1);
    std::shared_ptr<AudioSubGroup> getAudioSubGroupById(int groupId) const;
    
    std::shared_ptr<AudioSubGroup> getDefaultSubGroup() const;
    
    std::vector<std::shared_ptr<AudioSubGroup>> getAudioSubGroups() const { return audioSubGroups; }
    
    void setSelected(bool bSelected) { selected = bSelected; }
    bool isSelected() const { return selected; }
    
    void setChannelHeight(int height);
    
    std::vector<std::shared_ptr<PositionableBase>> getPositionableItems(bool arrangementMode) const;
    
    void deleteSelectedSubGroups();
    void deleteSubGroup(int atIndex);
    void deselectAllSubGroups();
    
    void deselectAllChannels();
    void deleteSelectedChannels();
    void deleteChannel(std::shared_ptr<AudioChannel> channel);
    
    juce::SparseSet<int> getSelectedRows() const;
    void setSelectedRows(juce::SparseSet<int>& selectedRows);
    
private:
    AudioGroupContainer &owner;
    AudioResourceContainer &audioResourceContainer;
    AudioRegionContainer &audioRegionContainer;
    std::shared_ptr<PlayListContainer> playListContainer;
    std::shared_ptr<TransportSourceContainer> transportSourceContainer;
    juce::String groupName;
    int groupId = -1;
    juce::Colour currentColour = juce::Colours::pink;
    
    std::vector<std::shared_ptr<AudioSubGroup>> audioSubGroups;
    int nextSubGroupId = 0;
    
    std::vector<std::shared_ptr<AudioChannel>> audioChannels;
    
    bool selected = false;
    
    bool subGroupIdExists(const int groupId) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGroup)

};
