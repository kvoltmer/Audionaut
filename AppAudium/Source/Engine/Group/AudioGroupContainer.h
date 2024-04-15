/*
  ==============================================================================

    AudioGroupContainer.h
    Created: 10 Oct 2023 12:12:23pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/Streamable.h"
#include "Engine/TimeContext.h"
#include "Engine/Region/AudioRegionData.h"
#include "Engine/Group/AudioRegionAdapter.h"

class AudioGroup;
class AudioResourceContainer;
class AudioRegionContainer;
class AudiumEngine;
class TempoProvider;
class AudioRegion;

class AudioGroupContainer : public juce::ActionBroadcaster,
                            public audium::Streamable,
                            public std::enable_shared_from_this<AudioGroupContainer>
{
        
public:
    
    AudioGroupContainer(std::shared_ptr<juce::UndoManager> undoManager,
                        std::shared_ptr<TempoProvider> tempoProvider) :
        undoManager(undoManager),
        tempoProvider(tempoProvider),
        audioRegionAdapter(*this)
    {
    }
    
    ~AudioGroupContainer();
    
    std::shared_ptr<AudioGroupContainer> getptr()
    {
        return shared_from_this();
    }
    
    void init(AudioResourceContainer *audioResourceContainer);
    
    bool groupIdExists(const int groupId) const;
        
    std::shared_ptr<AudioGroup> createNewAudioGroup(AudioResourceContainer &audioResourceContainer,
                                                    const juce::String nameString);
    void cleanup();
    
    bool deleteAudioGroup(std::shared_ptr<AudioGroup> group);
    void deleteSelectedGroups();
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream) override;
    bool writeToJson (json& output) override;
    bool readFromJson (json& input) override;
    int getSizeInUnits() override;
    
    std::shared_ptr<AudioGroup> getSelectedGroup() const { return audioGroups[selectedGroup]; }
    
    int getNumItems() const { return static_cast<int>(audioGroups.size());}
    std::shared_ptr<AudioGroup> getAudioGroup(int index) const;
    std::shared_ptr<AudioGroup> getSharedPtr(const AudioGroup* g) const;
        
    std::shared_ptr<AudioGroup> getDefaultGroup() const;
    
    std::vector<std::shared_ptr<AudioGroup>> getAudioGroups() const { return audioGroups; }
        
    void deselectAll();
    juce::SparseSet<int> getSelectedRows() const;
    void setSelectedRows(juce::SparseSet<int>& selectedRows);
    
    std::shared_ptr<TempoProvider> getTempoProvider() const noexcept { return tempoProvider; }
    std::shared_ptr<juce::UndoManager> getUndoManager() const noexcept { return undoManager; }
    
    void createRegionsFromSelection(juce::String name);
    
    // Used by RegionSelector
    void setSelectedPosition(juce::Range<double> pos, audium::TimeContextType context);
    juce::Range<double> getSelectedPosition(audium::TimeContextType context) const;
    

    AudioRegionAdapter &getAudioRegionAdapter() { return audioRegionAdapter; }
    
private:
    std::shared_ptr<juce::UndoManager> undoManager;
    std::shared_ptr<TempoProvider> tempoProvider;
    
    // i don't like this pointer :(
    AudioResourceContainer *audioResourceContainer = nullptr;
    
    std::vector<std::shared_ptr<AudioGroup>> audioGroups;
    int selectedGroup = 0;
    
    // TODO: capsulte this
    AudioRegionData::tRange selectedPositionClocks;
    
    // Discuss: inject depenendency
    AudioRegionAdapter audioRegionAdapter;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGroupContainer)
};
