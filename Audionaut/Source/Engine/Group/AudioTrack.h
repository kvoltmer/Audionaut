//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/Streamable.h"
#include "Engine/TimeContext.h"
#include "Engine/Core/DspClipData.h"
#include "Engine/Core/AudioClipContainer.h"
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/Selection/SelectableObjectContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/PlayList/PlayListContainer.h"

namespace audium {

class AudioResourceContainer;
class AudioResource;
class PlayListItem;
class PlayListScheduler;
class TransportSourceContainer;
class ResourceGroup;
class AudioChannel;
class PositionableBase;
class AudioTrackContainer;

typedef audium::SelectableObjectContainer<ResourceGroup> tResourceGroupContainer;
typedef audium::SelectableObjectContainer<AudioChannel> tAudioChannelContainer;

class AudioTrack : public audium::Streamable, public audium::Selectable
{
    
public:
    AudioTrack(AudioTrackContainer &owner_,
               AudioResourceContainer &audioResourceContainer_,
               std::shared_ptr<TransportSourceContainer> transportSourceContainer_,
               std::shared_ptr<SelectionManager> selectionManager_,
               std::shared_ptr<tResourceGroupContainer> resourceGroups_,
               std::shared_ptr<tAudioChannelContainer> channels_,
               juce::String nameString_) :
    audium::Selectable(selectionManager_),
    owner(owner_),
    audioResourceContainer(audioResourceContainer_),
    transportSourceContainer(transportSourceContainer_),
    selectionManager(selectionManager_),
    resourceGroupContainer(resourceGroups_),
    audioChannelContainer(channels_),
    name(nameString_.toStdString())
    {
        playListContainer = std::shared_ptr<PlayListContainer> (new PlayListContainer(*this,
                                                                                      owner.getTempoProvider(),
                                                                                      transportSourceContainer,
                                                                                      selectionManager));
    }
    
    virtual ~AudioTrack() override;
    
    void cleanup() override;
    
    const juce::String getAudioTrackName() const { return name; }
    void setAudioTrackName(const juce::String newName);
    
    // track colour:
    void setColour(juce::Colour colour);
    juce::Colour getColour() const { return groupColour; }
    
    // returns the index within the container's array
    const int getId() const;
    
    // returns the audio track's channel offset
    const int getChannelOffset() const;
    
    // pointer and references to other classes:
    AudioResourceContainer &getAudioResourceContainer() const { return audioResourceContainer; }
    AudioTrackContainer &getAudioTrackContainer() const { return owner; }
    std::shared_ptr<PlayListContainer> getPlayListContainer() const { return playListContainer; }
    std::shared_ptr<TransportSourceContainer> getTransportSourceContainer() const { return transportSourceContainer; }
    std::shared_ptr<SelectionManager> getSelectionManager() const noexcept { return selectionManager; }
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;
    std::shared_ptr<ResourceGroup> getResourceGroupAtAbsoluteRange(juce::Range<double> range, audium::TimeContextType context) const;
    std::shared_ptr<ResourceGroup> getResourceGroupAtAbsolutePosition(double position, audium::TimeContextType context) const;
    
    
    // audium::Streamable overrides:
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream, bool rebuild) override;
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    int getSizeInUnits() override;
    bool writeChannelToJson (json& output, AudioChannel* audioChannel);
    void mergeChannelFromJson(json& input);
    
    // audium::Selectable override:
    void setSelected(bool bSelected, bool selectChildren) override;
    
    void setGain(float gain, int channelNumber);
    float getGain(int channelNumber) const;
    
    void setPan(float pan, int channelNumber);
    float getPan(int channelNumber) const;
    
    void setMute(bool bMute, int channelNumber);
    bool getMute(int channelNumber) const;
    
    void setSolo(bool bSolo, int channelNumber);
    bool getSolo(int channelNumber) const;
    
    // undo for continious parameters:
    void onDragStart();
    void onDragEnd();
    
    // drag & drop:
    void dropSelectedAudioRegions(int insertIndex);
    void dropSelectedAudioRegions(double pos, audium::TimeContextType context);
    void dropPlayListItem(std::shared_ptr<PlayListItem> item, double pos, audium::TimeContextType context);
    
    // sub groups:
    std::shared_ptr<ResourceGroup> createNewResourceGroup(double transportPosition,
                                                          audium::TimeContextType context,
                                                          bool arrangementMode);
    std::shared_ptr<ResourceGroup> createNewResourceGroup(juce::Range<double> transportPositionRange, audium::TimeContextType context);
    std::shared_ptr<ResourceGroup> createNewResourceGroup(const std::shared_ptr<ResourceGroup> otherSubGroup);
    std::shared_ptr<ResourceGroup> findSimilarSubGroup(const std::shared_ptr<ResourceGroup> otherSubGroup);
    std::shared_ptr<ResourceGroup> getDefaultSubGroup() const;
    std::vector<std::shared_ptr<ResourceGroup>> getResourceGroups() const { return resourceGroupContainer->getObjects(); }
    
    // channel height:
    int getTotalHeight() const;
    void setChannelHeight(int height);
    
    std::list<std::shared_ptr<PositionableBase>> getPositionableItems(bool arrangementMode) const;
    
    // objects:
    bool deleteSelectedObject(std::shared_ptr<audium::Selectable> object, bool &rebuild);
    
    // channels:
    int getNumAudioTrackChannels() const;
    void ensureNumChannels(int channelsNeeded);
    std::shared_ptr<AudioChannel> addChannel();
    std::shared_ptr<AudioChannel> getChannel(int channelNumber) const;
    bool deleteChannel(AudioChannel* channel);
    void deleteEmptyResourceGroups();
    void deleteUnusedResourceGroups();
    
    bool addAudioFiles(const juce::StringArray& filenames,
                       double positionClocks,
                       bool arrangementMode,
                       std::function<void (std::string)> callback);
    
    std::vector<std::shared_ptr<AudioResource>> addAudioFile (std::shared_ptr<ResourceGroup> resourceGroup,
                                                              const juce::File filename,
                                                              int &destChannel);
    
    void createDefaultPlayListItem(std::shared_ptr<AudioResource> audioResource,
                                   std::shared_ptr<ResourceGroup> resourceGroup,
                                   double position,
                                   audium::TimeContextType context);
    
    double getTotalLength(audium::TimeContextType context, bool arrangementMode) const;
    
    std::vector<audium::DspClipData> getDspClipVector(bool arrangementMode) const;
    
    std::shared_ptr<AudioRegion> getRegion(int rowNumber) const;
    const std::vector<std::shared_ptr<AudioRegion>> getRegions() const;
    
private:
    AudioTrackContainer &owner;
    AudioResourceContainer &audioResourceContainer;
    std::shared_ptr<PlayListContainer> playListContainer;
    std::shared_ptr<TransportSourceContainer> transportSourceContainer;
    std::shared_ptr<SelectionManager> selectionManager;
    
public:
    std::shared_ptr<tResourceGroupContainer> resourceGroupContainer;
    std::shared_ptr<tAudioChannelContainer> audioChannelContainer;
    
private:
    
    std::string name;
    
    juce::Colour groupColour = juce::Colours::pink;
    
    std::unique_ptr<audium::UndoableContainerAction> undoableContainerAction;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrack)
    
};

} // namespace audium

