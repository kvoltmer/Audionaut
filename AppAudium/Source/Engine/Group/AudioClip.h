/*
  ==============================================================================

    AudioClip.h
    Created: 8 Feb 2024 4:27:13pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "Engine/Group/AudioSubGroup.h"
#include "Engine/Streamable.h"
#include "Engine/PlayList/PositionableBase.h"

class AudioClip :   public PositionableBase,
                    public audium::Streamable
{
    
public:
    AudioClip(AudioSubGroup &audioSubGroup) :
        audioSubGroup(audioSubGroup)
    {
    }
    
    juce::Range<double> getAbsolutePositionRange(audium::TimeContextType context) const override;
    double getAbsolutePosition(audium::TimeContextType context) const override;
    void setAbsolutePosition(double position, audium::TimeContextType context) override;
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream) override;
    int getSizeInUnits() override { return 1; }
    
    const juce::Range<double> getRegionData(audium::TimeContextType context) const;
    
    void setRegionData(const juce::Range<double> newRegionData, audium::TimeContextType context);
    
    bool validateData();
    
    AudioGroup &getAudioGroup() const { return audioSubGroup.getAudioGroup(); }
    
    double getFileLength(audium::TimeContextType context) const;

private:
    AudioSubGroup &audioSubGroup;
    
    // The absolute transport position
    double absolutePositionClocks = 0.0;
    // The start and end in seconds
    juce::Range<double> regionData;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioClip)
    
};
