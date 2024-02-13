/*
  ==============================================================================

    AudioRegionContainer.h
    Created: 30 May 2023 10:16:35am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "Engine/AudioRegion.h"
#include "Engine/ActionMessages.h"

class AudioResourceContainer;
class AudioGroupContainer;
class PlayListScheduler;
class AudioGroup;
class AudioSubGroup;

class AudioRegionContainer :    public juce::ActionBroadcaster
{
                                            
public:
    AudioRegionContainer(std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                         std::shared_ptr<AudioGroupContainer> audioGroupContainer,
                         std::shared_ptr<PlayListScheduler> playListScheduler,
                         std::shared_ptr<juce::UndoManager> undoManager) :
        audioResourceContainer(audioResourceContainer),
        audioGroupContainer(audioGroupContainer),
        playListScheduler(playListScheduler),
        undoManager(undoManager)
    {}
    
    std::shared_ptr<AudioRegion> createDefaultRegion(std::shared_ptr<AudioGroup> group);
    std::shared_ptr<AudioRegion> createRegion(std::shared_ptr<AudioGroup> group,
                                              std::shared_ptr<AudioSubGroup> subGroup);
    
    std::shared_ptr<AudioRegion> createRegion(juce::String regionName,
                                              juce::Range<double> position,
                                              std::shared_ptr<AudioGroup> group,
                                              std::shared_ptr<AudioSubGroup> subGroup);
    void deleteRegion(int rowNumber);
    void deleteSelectedRegions();
    void createRegionsFromSelection(juce::String name);

    
    // Used by RegionSelector
    void setSelectedPosition(juce::Range<double> pos, audium::TimeContextType context);
    juce::Range<double> getSelectedPosition(audium::TimeContextType context) const;

    int getNumRegions(const AudioGroup* group = nullptr) const;
    std::shared_ptr<AudioRegion> getRegion(int index) const;
    int getRegionIndex(std::shared_ptr<AudioRegion> searchRegion) const;
    
    void deselectAll();
    juce::SparseSet<int> getSelectedRows() const;
    void setSelectedRows(juce::SparseSet<int>& selectedRows);

    void setRegionName(int rowNumber, juce::String newName);
    void setRegionStart(int rowNumber, double newStart);
    void setRegionEnd(int rowNumber, double newEnd);
    void setRegionLength(int rowNumber, double newLength);
    
    void cleanup() { audioRegions.clear(); }
    
    std::vector<std::shared_ptr<AudioRegion>> getRegionsForGroup(std::shared_ptr<AudioGroup> group) const;
    std::vector<std::shared_ptr<AudioRegion>> getRegionsForSubGroup(const AudioSubGroup* subGroup) const;
    void deleteAudioRegion(std::shared_ptr<AudioRegion> region);
    void deleteAudioRegionsForGroup(std::shared_ptr<AudioGroup> group);
    void deleteAudioRegionsForSubGroup(std::shared_ptr<AudioSubGroup> audioSubGroup);

    
    std::vector<std::shared_ptr<AudioRegion>> getRegionsForResource(std::shared_ptr<AudioResource> audioResource) const;
    
    std::shared_ptr<PlayListScheduler> getPlayListScheduler() const { return playListScheduler; }
    std::shared_ptr<juce::UndoManager> getUndoManager() const { return undoManager; }
    std::shared_ptr<AudioGroupContainer> getAudioGroupContainer() const { return audioGroupContainer; }
    
private:
    
    
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<AudioGroupContainer> audioGroupContainer;
    std::shared_ptr<PlayListScheduler> playListScheduler;
    std::shared_ptr<juce::UndoManager> undoManager;

    AudioRegion::RegionData selectedPositionClocks;
    int selectedRowNumber = -1;
    
    std::vector<std::shared_ptr<AudioRegion>> audioRegions;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionContainer)
};
