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
    
    std::shared_ptr<AudioGroup> createNewAudioGroup(const AudioResourceContainer &audioResourceContainer,
                                                    const AudioRegionContainer &audioRegionContainer,
                                                    std::string nameString);
    void cleanup();
    
    bool removeAudioGroup(std::shared_ptr<AudiumEngine> engine, std::shared_ptr<AudioGroup> group);
    
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);
    
    std::shared_ptr<AudioGroup> getSelectedGroup() const { return audioGroups[selectedGroup]; }
    
    int getNumItems() const { return static_cast<int>(audioGroups.size());}
    std::shared_ptr<AudioGroup> getAudioGroup(int index) { return audioGroups[index]; }
    
private:
    
    std::vector<std::shared_ptr<AudioGroup>> audioGroups;
    int selectedGroup = 0;

};
