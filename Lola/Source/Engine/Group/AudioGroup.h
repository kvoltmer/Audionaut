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
#include "Engine/TimeContext.h"
#include "Engine/Core/DspClipData.h"
#include "Engine/Core/AudioClipContainer.h"

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
               std::shared_ptr<AudioRegionContainer> audioRegionContainer,
               std::shared_ptr<PlayListContainer> playListContainer,
               std::shared_ptr<TransportSourceContainer> transportSourceContainer,
               juce::String nameString) :
        owner(owner),
        audioResourceContainer(audioResourceContainer),
        audioRegionContainer(audioRegionContainer),
        playListContainer(playListContainer),
        transportSourceContainer(transportSourceContainer),
        groupName(nameString.toStdString())
    {
    }
    
    ~AudioGroup();
    
    void cleanup();
    

    const juce::String getName() const { return groupName; }
    void setName(const juce::String newName) { groupName = newName.toStdString(); }
    
    AudioResourceContainer &getAudioResourceContainer() const { return audioResourceContainer; }
    AudioGroupContainer &getAudioGroupContainer() const { return owner; }
    std::shared_ptr<AudioRegionContainer> getAudioRegionContainer() const { return audioRegionContainer; }
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesAtChannelPosition(int channelPosition) const;
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesAtAbsoluteRange(juce::Range<double> rangeInSeconds) const;
    std::shared_ptr<AudioSubGroup> getSubGroupAtAbsolutePosition(double position, audium::TimeContextType context) const;
    
    void setColour(juce::Colour colour);
    juce::Colour getColour() const { return groupColour; }
    
    std::shared_ptr<PlayListContainer> getPlayListContainer() const { return playListContainer; }
    std::shared_ptr<TransportSourceContainer> getTransportSourceContainer() const { return transportSourceContainer; }
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream, bool rebuild) override;
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    int getSizeInUnits() override;
    
    int getNumChannels() const;
    void ensureNumChannels(int channelsNeeded);
    std::shared_ptr<AudioChannel> addChannel();
    std::shared_ptr<AudioChannel> getChannel(int channelNumber) const;

    int getTotalHeight() const;
    float getOutputLevel(int channelNumber) const;
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesAtChannel(int channelNumber) const;
    
    void setGain(float gain, int channelNumber);
    float getGain(int channelNumber) const;
    
    std::shared_ptr<AudioSubGroup> createNewAudioSubGroup(double transportPosition, audium::TimeContextType context);
    std::shared_ptr<AudioSubGroup> getSharedPtr(const AudioSubGroup* subGroup) const;
    
    std::shared_ptr<AudioSubGroup> getDefaultSubGroup() const;
    
    std::vector<std::shared_ptr<AudioSubGroup>> getAudioSubGroups() const { return audioSubGroups; }
    
    void setSelected(bool bSelected);
    bool isSelected() const { return selected; }
    
    void setChannelHeight(int height);
    
    std::list<std::shared_ptr<PositionableBase>> getPositionableItems(bool arrangementMode) const;
    
    void deleteSelectedSubGroups();
    void deleteSubGroup(int atIndex);
    
    void selectAllSubGroups(bool bSelected);
    
    void selectAllChannels(bool bSelected);
    
    void deleteSelectedChannels();
    void deleteChannel(std::shared_ptr<AudioChannel> channel);
    
    juce::SparseSet<int> getSelectedRows() const;
    void setSelectedRows(juce::SparseSet<int>& selectedRows);
    
    bool addAudioFiles(const juce::StringArray& filenames,
                       double positionClocks,
                       bool arrangementMode,
                       std::function<void (std::string)> callback);
    
    std::shared_ptr<AudioResource> addAudioFile(std::shared_ptr<AudioSubGroup> subGroup,
                                                const juce::File filename,
                                                int &channelPosition);
    
    void createDefaultPlayListItem(std::shared_ptr<AudioResource> audioResource,
                                   std::shared_ptr<AudioSubGroup> subGroup,
                                   double position,
                                   audium::TimeContextType context);
    
    double getTotalLength(audium::TimeContextType context, bool arrangementMode) const;
    
    std::vector<DspClipData> getDspClipVector(bool arrangementMode) const;
    
private:
    AudioGroupContainer &owner;
    AudioResourceContainer &audioResourceContainer;
    std::shared_ptr<AudioRegionContainer> audioRegionContainer;
    std::shared_ptr<PlayListContainer> playListContainer;
    std::shared_ptr<TransportSourceContainer> transportSourceContainer;
    std::string groupName;
    juce::Colour groupColour = juce::Colours::pink;
    
    std::vector<std::shared_ptr<AudioSubGroup>> audioSubGroups;
    
    std::vector<std::shared_ptr<AudioChannel>> audioChannels;
    
    bool selected = false;
        
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGroup)

};
