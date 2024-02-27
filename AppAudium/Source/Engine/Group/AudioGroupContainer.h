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

class AudioGroup;
class AudioResourceContainer;
class AudioRegionContainer;
class AudiumEngine;
class TempoProvider;

class AudioGroupContainer : public juce::ActionBroadcaster,
                            public audium::Streamable
{
        
public:
    
    AudioGroupContainer(std::shared_ptr<juce::UndoManager> undoManager,
                        std::shared_ptr<TempoProvider> tempoProvider) :
        undoManager(undoManager),
        tempoProvider(tempoProvider)
    {
    }
    
    ~AudioGroupContainer();
    
    void init(AudioResourceContainer *audioResourceContainer,
              AudioRegionContainer *audioRegionContainer);
    
    bool groupIdExists(const int groupId) const;
        
    std::shared_ptr<AudioGroup> createNewAudioGroup(AudioResourceContainer &audioResourceContainer,
                                                    AudioRegionContainer &audioRegionContainer,
                                                    const juce::String nameString,
                                                    int groupId = -1);
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
    std::shared_ptr<AudioGroup> getAudioGroupById(int groupId) const;
    
    int getNextId() { return ++nextId; }
        
    std::shared_ptr<AudioGroup> getDefaultGroup() const;
    
    std::vector<std::shared_ptr<AudioGroup>> getAudioGroups() const { return audioGroups; }
    
    void deselectAll();
    juce::SparseSet<int> getSelectedRows() const;
    void setSelectedRows(juce::SparseSet<int>& selectedRows);
    
    AudioRegionContainer *getAudioRegionContainer() const noexcept { return audioRegionContainer; }
    std::shared_ptr<TempoProvider> getTempoProvider() const noexcept { return tempoProvider; }
    std::shared_ptr<juce::UndoManager> getUndoManager() const noexcept { return undoManager; }
    
private:
    std::shared_ptr<juce::UndoManager> undoManager;
    std::shared_ptr<TempoProvider> tempoProvider;
    
    // i don't like these pointers :(
    AudioResourceContainer *audioResourceContainer = nullptr;
    AudioRegionContainer *audioRegionContainer = nullptr;
    
    std::vector<std::shared_ptr<AudioGroup>> audioGroups;
    int selectedGroup = 0;
    
    int nextId = 0;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGroupContainer)
};
