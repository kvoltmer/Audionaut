/*
  ==============================================================================

    AudioSubGroup.h
    Created: 19 Dec 2023 3:47:13pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/TimeContext.h"
#include "Engine/PlayList/PositionableBase.h"

class AudioGroup;
class AudioResource;


class AudioSubGroup : public PositionableBase
{
        
public:
    AudioSubGroup(AudioGroup& audioGroup, int subGroupId) :
        audioGroup(audioGroup),
        subGroupId(subGroupId)
    {}
    
    const int getId() const noexcept { return subGroupId; }
    void setId(const int newId) { subGroupId = newId; }
    
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;
    
    int getNumChannels() const;
    std::shared_ptr<AudioResource> getChannel(int rowNumber) const;

    AudioGroup& getAudioGroup() const { return audioGroup; }
    
    void setSelected(bool bSelected) { selected = bSelected; }
    bool isSelected() const { return selected; }
    
    juce::Range<double> getAbsolutePosition(audium::TimeContextType context) const override;

private:
    AudioGroup& audioGroup;

    int subGroupId = -1;
    bool selected = false;
};
