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
#include "Engine/Streamable.h"

class AudioGroup;
class AudioResource;
class AudioRegion;

class AudioSubGroup :   public PositionableBase,
                        public audium::Streamable
{
        
public:
    AudioSubGroup(AudioGroup& audioGroup, int subGroupId = -1) :
        audioGroup(audioGroup),
        subGroupId(subGroupId)
    {}
    
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
    
    juce::Range<double> getAbsolutePositionRange(audium::TimeContextType context) const override;
    double getAbsolutePosition(audium::TimeContextType context) const override;
    void setAbsolutePosition(double position, audium::TimeContextType context) override;

private:
    AudioGroup& audioGroup;

    int subGroupId = -1;
    bool selected = false;
    
    double absolutePositionClocks = 0.0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSubGroup)

};
