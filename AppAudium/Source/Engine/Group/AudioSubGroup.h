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
#include "Engine/Streamable.h"

class AudioGroup;
class AudioResource;
class AudioRegion;
class AudioClip;

class AudioSubGroup : public audium::Streamable
{
        
public:
    AudioSubGroup(AudioGroup& audioGroup, int subGroupId = -1);
    ~AudioSubGroup() override;
    void cleanup();
    
    const int getId() const noexcept { return subGroupId; }
    void setId(const int newId) { subGroupId = newId; }
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream) override;
    int getSizeInUnits() override;
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;
    
    std::vector<std::shared_ptr<AudioRegion>> getAudioRegions() const;
    
    int getNumChannels() const;
    std::shared_ptr<AudioResource> getChannel(int rowNumber) const;

    AudioGroup& getAudioGroup() const { return audioGroup; }
    
    void setSelected(bool bSelected) { selected = bSelected; }
    bool isSelected() const { return selected; }


    std::shared_ptr<AudioClip> getAudioClip() const { return audioClip; }
    
private:
    std::shared_ptr<AudioClip> audioClip;
    
    AudioGroup& audioGroup;

    int subGroupId = -1;
    bool selected = false;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSubGroup)

};
