/*
  ==============================================================================

    AudioSubGroup.h
    Created: 19 Dec 2023 3:47:13pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class AudioGroup;
//class AudioResourceContainer;
//class AudioRegionContainer;
//class AudiumEngine;
//class AudioGroupContainer;

class AudioSubGroup {
        
public:
    AudioSubGroup(const AudioGroup& audioGroup, int subGroupId) :
        audioGroup(audioGroup),
        subGroupId(subGroupId)
    {}
    
    const int getId() const noexcept { return subGroupId; }
    void setId(const int newId) { subGroupId = newId; }
    
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);
    
private:
    const AudioGroup& audioGroup;

    int subGroupId = -1;
};
