/*
  ==============================================================================

    AudioGroupContainer.h
    Created: 10 Oct 2023 12:12:23pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class AudioGroup;
class AudioResourceContainer;
class AudioRegionContainer;
class AudiumEngine;

class AudioGroupContainer : public juce::ActionBroadcaster
{
        
public:
    
    AudioGroupContainer() = default;
    ~AudioGroupContainer();
    
    bool groupIdExists(const int groupId) const;
    
    static std::shared_ptr<AudioGroup> createAudioGroup(const AudioResourceContainer &audioResourceContainer,
                                                        const AudioRegionContainer &audioRegionContainer);
    
    std::shared_ptr<AudioGroup> createNewAudioGroup(const AudioResourceContainer &audioResourceContainer,
                                                    const AudioRegionContainer &audioRegionContainer,
                                                    std::string nameString,
                                                    int groupId = -1);
    void cleanup();
    
    bool removeAudioGroup(std::shared_ptr<AudiumEngine> engine, std::shared_ptr<AudioGroup> group);
    
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream,
                         const AudioResourceContainer &audioResourceContainer,
                         const AudioRegionContainer &audioRegionContainer);
    
    std::shared_ptr<AudioGroup> getSelectedGroup() const { return audioGroups[selectedGroup]; }
    
    int getNumItems() const { return static_cast<int>(audioGroups.size());}
    std::shared_ptr<AudioGroup> getAudioGroup(int index) const;
    std::shared_ptr<AudioGroup> getAudioGroupById(int groupId) const;
    
    int getNextId() { return ++nextId; }
    
private:
    
    std::vector<std::shared_ptr<AudioGroup>> audioGroups;
    int selectedGroup = 0;
    
    int nextId = 0;
    

};
