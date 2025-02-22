/*
  ==============================================================================

    AudioRegionContainer.h
    Created: 30 May 2023 10:16:35am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "Engine/Region/AudioRegion.h"
#include "Engine/ActionMessages.h"

class AudioResourceContainer;
class AudioTrackContainer;
class AudioTrack;
class AudioSubGroup;

class AudioRegionContainer : public audium::Streamable
{
                                            
public:
    AudioRegionContainer(AudioResourceContainer &audioResourceContainer_,
                         AudioTrackContainer &audioTrackContainer_,
                         std::shared_ptr<TempoProvider> tempoProvider_,
                         std::shared_ptr<juce::UndoManager> undoManager_) :
        audioResourceContainer(audioResourceContainer_),
        audioTrackContainer(audioTrackContainer_),
        tempoProvider(tempoProvider_),
        undoManager(undoManager_)
    {}
    
    std::shared_ptr<AudioRegion> createDefaultRegion(std::shared_ptr<AudioTrack> track);
    std::shared_ptr<AudioRegion> createRegion(std::shared_ptr<AudioTrack> track,
                                              std::shared_ptr<AudioSubGroup> subGroup);
    
    std::shared_ptr<AudioRegion> createRegion(juce::String regionName,
                                              juce::Range<double> position,
                                              std::shared_ptr<AudioTrack> track,
                                              std::shared_ptr<AudioSubGroup> subGroup,
                                              std::shared_ptr<AudioRegion> otherRegion,
                                              audium::TimeContextType context);
    
    std::shared_ptr<AudioRegion> createRegion(std::shared_ptr<AudioTrack> track,
                                              const std::shared_ptr<AudioRegion> otherRegion);
    
    static std::string formatNumber(long num);
    const juce::String getUniqueName(juce::String regionName) const;
    
    void cleanup();
    
    int getNumRegions(const AudioTrack* track = nullptr) const;
    std::shared_ptr<AudioRegion> getRegion(int index) const;
    int getRegionId(std::shared_ptr<AudioRegion> searchRegion) const;
    
    std::vector<std::shared_ptr<AudioRegion>> getRegionsForSubGroup(const AudioSubGroup* subGroup) const;
    std::vector<std::shared_ptr<AudioRegion>> getSelectedRegions(bool global = false) const;
    void deleteAudioRegion(std::shared_ptr<AudioRegion> region);
    bool deleteAudioRegion(AudioRegion* region);
    void deleteAudioRegionsForSubGroup(std::shared_ptr<AudioSubGroup> audioSubGroup);
    void deleteUnusedRegions();
    void sortRegionIds();

    std::vector<std::shared_ptr<AudioRegion>> getRegionsForResource(std::shared_ptr<AudioResource> audioResource) const;
    
    juce::SparseSet<int> getSelectedRows() const;
    void setSelectedRows(juce::SparseSet<int>& selectedRows);
    
    std::shared_ptr<AudioRegion> getRegionWithData(const AudioRegionData &data) const;
    
    std::shared_ptr<juce::UndoManager> getUndoManager() const { return undoManager; }
    AudioTrackContainer& getAudioTrackContainer() const { return audioTrackContainer; }
    AudioResourceContainer& getAudioResourceContainer() const { return audioResourceContainer; }
    
    
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    int getSizeInUnits() override { return static_cast<int>(audioRegions.size() * 2); }
    void mergeFromJson(json& input);

    const std::vector<std::shared_ptr<AudioRegion>> &getObjects() const { return audioRegions; }
    
private:
    
    AudioResourceContainer &audioResourceContainer;
    AudioTrackContainer &audioTrackContainer;
    std::shared_ptr<TempoProvider> tempoProvider;
    std::shared_ptr<juce::UndoManager> undoManager;

    int selectedRowNumber = -1;
    
    std::vector<std::shared_ptr<AudioRegion>> audioRegions;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionContainer)
};
