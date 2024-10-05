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
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"

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

class AudioGroup : public audium::Streamable, public audium::Selectable
{
    
public:
    AudioGroup(AudioGroupContainer &owner,
               AudioResourceContainer &audioResourceContainer,
               std::shared_ptr<AudioRegionContainer> audioRegionContainer,
               std::shared_ptr<PlayListContainer> playListContainer,
               std::shared_ptr<TransportSourceContainer> transportSourceContainer,
               std::shared_ptr<audium::SelectionManager> selectionManager,
               juce::String nameString) :
        audium::Selectable(selectionManager),
        owner(owner),
        audioResourceContainer(audioResourceContainer),
        audioRegionContainer(audioRegionContainer),
        playListContainer(playListContainer),
        transportSourceContainer(transportSourceContainer),
        selectionManager(selectionManager),
        groupName(nameString.toStdString())
    {
    }
    
    virtual ~AudioGroup() override;
    
    void cleanup();
    
    // group name:
    const juce::String getName() const { return groupName; }
    void setName(const juce::String newName) { groupName = newName.toStdString(); }
    
    // group colour:
    void setColour(juce::Colour colour);
    juce::Colour getColour() const { return groupColour; }
    
    // pointer and references to other classes:
    AudioResourceContainer &getAudioResourceContainer() const { return audioResourceContainer; }
    AudioGroupContainer &getAudioGroupContainer() const { return owner; }
    std::shared_ptr<AudioRegionContainer> getAudioRegionContainer() const { return audioRegionContainer; }
    std::shared_ptr<PlayListContainer> getPlayListContainer() const { return playListContainer; }
    std::shared_ptr<TransportSourceContainer> getTransportSourceContainer() const { return transportSourceContainer; }
    std::shared_ptr<audium::SelectionManager> getSelectionManager() const noexcept { return selectionManager; }
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesAtChannelPosition(int channelPosition) const;
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesAtAbsoluteRange(juce::Range<double> rangeInSeconds) const;
    std::shared_ptr<AudioSubGroup> getSubGroupAtAbsolutePosition(double position, audium::TimeContextType context) const;
    

    // audium::Streamable overrides:
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream, bool rebuild) override;
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    int getSizeInUnits() override;
   
    // audium::Selectable override:
    void setSelected(bool bSelected, bool selectChildren) override;
    

    float getOutputLevel(int channelNumber) const;
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesAtChannel(int channelNumber) const;
    
    void setGain(float gain, int channelNumber);
    float getGain(int channelNumber) const;
    
    // sub groups:
    std::shared_ptr<AudioSubGroup> createNewAudioSubGroup(double transportPosition, audium::TimeContextType context);
    std::shared_ptr<AudioSubGroup> getSharedPtr(const AudioSubGroup* subGroup) const;
    std::shared_ptr<AudioSubGroup> getDefaultSubGroup() const;
    std::vector<std::shared_ptr<AudioSubGroup>> getAudioSubGroups() const { return audioSubGroups; }
    bool deleteSubGroup(AudioSubGroup* subGroup);
    void deleteSubGroup(int atIndex);
    void selectAllSubGroups(bool bSelected);
    
    // channel height:
    int getTotalHeight() const;
    void setChannelHeight(int height);
    
    std::list<std::shared_ptr<PositionableBase>> getPositionableItems(bool arrangementMode) const;
    
    // objects:
    bool deleteSelectedObject(std::shared_ptr<audium::Selectable> object);
    
    // channels:
    int getNumChannels() const;
    void ensureNumChannels(int channelsNeeded);
    std::shared_ptr<AudioChannel> addChannel();
    std::shared_ptr<AudioChannel> getChannel(int channelNumber) const;
    void selectAllChannels(bool bSelected);
    bool deleteChannel(AudioChannel* channel);
    
    
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
    std::shared_ptr<audium::SelectionManager> selectionManager;
    
    std::string groupName;
    
    juce::Colour groupColour = juce::Colours::pink;
    
    std::vector<std::shared_ptr<AudioSubGroup>> audioSubGroups;
    
    std::vector<std::shared_ptr<AudioChannel>> audioChannels;
        
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGroup)

};
