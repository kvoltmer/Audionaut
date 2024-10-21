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
class PlayListScheduler;
class AudioTrack;
class AudioSubGroup;

class AudioRegionContainer
{
                                            
public:
    AudioRegionContainer(AudioResourceContainer &audioResourceContainer,
                         AudioTrackContainer &audioTrackContainer,
                         std::shared_ptr<TempoProvider> tempoProvider,
                         std::shared_ptr<juce::UndoManager> undoManager) :
        audioResourceContainer(audioResourceContainer),
        audioTrackContainer(audioTrackContainer),
        tempoProvider(tempoProvider),
        undoManager(undoManager)
    {}
    
    std::shared_ptr<AudioRegion> createDefaultRegion(std::shared_ptr<AudioTrack> track);
    std::shared_ptr<AudioRegion> createRegion(std::shared_ptr<AudioTrack> track,
                                              std::shared_ptr<AudioSubGroup> subGroup);
    
    std::shared_ptr<AudioRegion> createRegion(juce::String regionName,
                                              juce::Range<double> position,
                                              std::shared_ptr<AudioTrack> track,
                                              std::shared_ptr<AudioSubGroup> subGroup);
    
    void cleanup();
    
    int getNumRegions(const AudioTrack* track = nullptr) const;
    std::shared_ptr<AudioRegion> getRegion(int index) const;
    int getRegionId(std::shared_ptr<AudioRegion> searchRegion) const;
    
    std::vector<std::shared_ptr<AudioRegion>> getRegionsForSubGroup(const AudioSubGroup* subGroup) const;
    void deleteAudioRegion(std::shared_ptr<AudioRegion> region);
    bool deleteAudioRegion(AudioRegion* region);
    void deleteAudioRegionsForSubGroup(std::shared_ptr<AudioSubGroup> audioSubGroup);

    std::vector<std::shared_ptr<AudioRegion>> getRegionsForResource(std::shared_ptr<AudioResource> audioResource) const;
    
    std::shared_ptr<juce::UndoManager> getUndoManager() const { return undoManager; }
    AudioTrackContainer& getAudioTrackContainer() const { return audioTrackContainer; }
    AudioResourceContainer& getAudioResourceContainer() const { return audioResourceContainer; }
private:
    
    AudioResourceContainer &audioResourceContainer;
    AudioTrackContainer &audioTrackContainer;
    std::shared_ptr<TempoProvider> tempoProvider;
    std::shared_ptr<juce::UndoManager> undoManager;

    int selectedRowNumber = -1;
    
    std::vector<std::shared_ptr<AudioRegion>> audioRegions;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionContainer)
};
