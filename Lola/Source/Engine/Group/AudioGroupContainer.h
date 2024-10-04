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
#include "Engine/Selection/SelectionManager.h"

class AudioGroup;
class AudioResourceContainer;
class AudioRegionContainer;
class AudiumEngine;
class TempoProvider;
class AudioRegion;
class TransportSourceContainer;
class AudioResourceContainer;

class AudioGroupContainer : public juce::ActionBroadcaster,
                            public audium::Streamable
{
        
public:
    
    AudioGroupContainer(std::shared_ptr<juce::UndoManager> undoManager,
                        std::shared_ptr<TempoProvider> tempoProvider,
                        std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                        std::shared_ptr<TransportSourceContainer> transportSourceContainer,
                        std::shared_ptr<audium::SelectionManager> selectionManager) :
        undoManager(undoManager),
        tempoProvider(tempoProvider),
        audioResourceContainer(audioResourceContainer),
        transportSourceContainer(transportSourceContainer),
        selectionManager(selectionManager),
        audioRegionAdapter(*this)
    {
    }
    
    ~AudioGroupContainer();
    
    bool groupIdExists(const int groupId) const;
        
    std::shared_ptr<AudioGroup> createNewAudioGroup(const juce::String nameString);
    void cleanup();
    
    bool deleteAudioGroup(AudioGroup* group);
    bool deleteAudioGroup(std::shared_ptr<AudioGroup> group);
    void deleteSelectedObjects();
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream, bool rebuild) override;
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    int getSizeInUnits() override;
    
    std::shared_ptr<AudioGroup> getSelectedGroup() const { return audioGroups[selectedGroup]; }
    
    int getNumItems() const { return static_cast<int>(audioGroups.size());}
    std::shared_ptr<AudioGroup> getAudioGroup(int index) const;
    std::shared_ptr<AudioGroup> getSharedPtr(const AudioGroup* g) const;
        
    std::shared_ptr<AudioGroup> getDefaultGroup() const;
    
    std::vector<std::shared_ptr<AudioGroup>> getAudioGroups() const { return audioGroups; }
        
    void selectAllGroups(bool bSelected, bool selectChildren);
    juce::SparseSet<int> getSelectedRows() const;
    void setSelectedRows(juce::SparseSet<int>& selectedRows);
    bool isSomethingSelected();
    
    std::shared_ptr<TempoProvider> getTempoProvider() const noexcept { return tempoProvider; }
    std::shared_ptr<juce::UndoManager> getUndoManager() const noexcept { return undoManager; }
    std::shared_ptr<TransportSourceContainer> getTransportSourceContainer() const noexcept { return transportSourceContainer; }
    std::shared_ptr<audium::SelectionManager> getSelectionManager() const noexcept { return selectionManager; }
    
    AudioRegionAdapter &getAudioRegionAdapter() { return audioRegionAdapter; }
    
private:
    std::shared_ptr<juce::UndoManager> undoManager;
    std::shared_ptr<TempoProvider> tempoProvider;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<TransportSourceContainer> transportSourceContainer;
    std::shared_ptr<audium::SelectionManager> selectionManager;
    
    std::vector<std::shared_ptr<AudioGroup>> audioGroups;
    int selectedGroup = 0;
    

    
    // Discuss: inject depenendency
    AudioRegionAdapter audioRegionAdapter;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGroupContainer)
};
