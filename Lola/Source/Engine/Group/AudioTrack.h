//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

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
class AudioSubGroup;
class AudioChannel;
class PositionableBase;
class AudioTrackContainer;

typedef audium::SelectableObjectContainer<AudioSubGroup> tAudioSubGroupContainer;
typedef audium::SelectableObjectContainer<AudioChannel> tAudioChannelContainer;

class AudioTrack : public audium::Streamable, public audium::Selectable
{
    
public:
    AudioTrack(AudioTrackContainer &owner,
               AudioResourceContainer &audioResourceContainer,
               std::shared_ptr<TransportSourceContainer> transportSourceContainer,
               std::shared_ptr<SelectionManager> selectionManager,
               std::shared_ptr<tAudioSubGroupContainer> subGroups,
               std::shared_ptr<tAudioChannelContainer> channels,
               juce::String nameString) :
    audium::Selectable(selectionManager),
    owner(owner),
    audioResourceContainer(audioResourceContainer),
    transportSourceContainer(transportSourceContainer),
    selectionManager(selectionManager),
    audioSubGroupContainer(subGroups),
    audioChannelContainer(channels),
    name(nameString.toStdString())
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
    std::shared_ptr<AudioSubGroup> getSubGroupAtAbsoluteRange(juce::Range<double> range, audium::TimeContextType context) const;
    std::shared_ptr<AudioSubGroup> getSubGroupAtAbsolutePosition(double position, audium::TimeContextType context) const;
    
    
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
    std::shared_ptr<AudioSubGroup> createNewAudioSubGroup(double transportPosition,
                                                          audium::TimeContextType context,
                                                          bool arrangementMode);
    std::shared_ptr<AudioSubGroup> createNewAudioSubGroup(juce::Range<double> transportPositionRange, audium::TimeContextType context);
    std::shared_ptr<AudioSubGroup> createNewAudioSubGroup(const std::shared_ptr<AudioSubGroup> otherSubGroup);
    std::shared_ptr<AudioSubGroup> findSimilarSubGroup(const std::shared_ptr<AudioSubGroup> otherSubGroup);
    std::shared_ptr<AudioSubGroup> getDefaultSubGroup() const;
    std::vector<std::shared_ptr<AudioSubGroup>> getAudioSubGroups() const { return audioSubGroupContainer->getObjects(); }
    
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
    void deleteEmptySubGroups();
    void deleteUnusedSubGroups();
    
    bool addAudioFiles(const juce::StringArray& filenames,
                       double positionClocks,
                       bool arrangementMode,
                       std::function<void (std::string)> callback);
    
    std::vector<std::shared_ptr<AudioResource>> addAudioFile (std::shared_ptr<AudioSubGroup> subGroup,
                                                              const juce::File filename,
                                                              int &destChannel);
    
    void createDefaultPlayListItem(std::shared_ptr<AudioResource> audioResource,
                                   std::shared_ptr<AudioSubGroup> subGroup,
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
    std::shared_ptr<tAudioSubGroupContainer> audioSubGroupContainer;
    std::shared_ptr<tAudioChannelContainer> audioChannelContainer;
    
private:
    
    std::string name;
    
    juce::Colour groupColour = juce::Colours::pink;
    
    std::unique_ptr<audium::UndoableContainerAction> undoableContainerAction;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrack)
    
};

} // namespace audium

