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

class AudioResourceContainer;
class AudioGroupContainer;
class PlayListScheduler;
class AudioGroup;
class AudioSubGroup;

class AudioRegionContainer : public juce::ActionBroadcaster
{
                                            
public:
    AudioRegionContainer(std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                         std::shared_ptr<AudioGroupContainer> audioGroupContainer,
                         std::shared_ptr<PlayListScheduler> playListScheduler) :
        audioResourceContainer(audioResourceContainer),
        audioGroupContainer(audioGroupContainer),
        playListScheduler(playListScheduler)
    {}
    
    std::shared_ptr<AudioRegion> createDefaultRegion(std::shared_ptr<AudioGroup> group);
    std::shared_ptr<AudioRegion> createRegion(juce::String regionName,
                                              juce::Range<double> position,
                                              std::shared_ptr<AudioGroup> group,
                                              std::shared_ptr<AudioSubGroup> subGroup = nullptr);
    void deleteRegion(int rowNumber);
    void createRegionsFromSelection(juce::String name);
    
    // Used by RegionSelector
    void setSelectedPositionInSeconds(juce::Range<double> pos);
    juce::Range<double> getSelectedPositionInSeconds() const;
    
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);
    
    int getNumRegions(const AudioGroup* group = nullptr) const;
    std::shared_ptr<AudioRegion> getRegion(int index) const;
    int getRegionIndex(std::shared_ptr<AudioRegion> searchRegion) const;
    
    void setSelectedRegion(int rowNumber);
    int getSelectedRegion() const;
    void deselectAll();
    
    void setRegionName(int rowNumber, juce::String newName);
    void setRegionStart(int rowNumber, double newStart);
    void setRegionEnd(int rowNumber, double newEnd);
    void setRegionLength(int rowNumber, double newLength);
    
    void cleanup() { audioRegions.clear(); }
    
    std::vector<std::shared_ptr<AudioRegion>> getRegionsForGroup(std::shared_ptr<AudioGroup> group) const;
    void removeAudioRegion(std::shared_ptr<AudioRegion> region);
    void removeAudioRegionsForGroup(std::shared_ptr<AudioGroup> group);
    
    std::vector<std::shared_ptr<AudioRegion>> getRegionsForResource(std::shared_ptr<AudioResource> audioResource) const;
    
    std::shared_ptr<PlayListScheduler> getPlayListScheduler() const { return playListScheduler; }
        
    
private:
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<AudioGroupContainer> audioGroupContainer;
    std::shared_ptr<PlayListScheduler> playListScheduler;

    AudioRegion::RegionData selectedPositionClocks;
    int selectedRowNumber = -1;
    
    std::vector<std::shared_ptr<AudioRegion>> audioRegions;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionContainer)
};
