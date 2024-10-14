/*
  ==============================================================================

    AudioClip.h
    Created: 8 Feb 2024 4:27:13pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "Engine/Group/AudioClipData.h"
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
        
    double getAbsolutePosition(audium::TimeContextType context) const override;
    void setAbsolutePosition(double position, audium::TimeContextType context) override;
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream, bool rebuild) override;
    
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    
    int getSizeInUnits() override { return 1; }
    
    juce::Range<double> getRegionData(audium::TimeContextType context) const override;
    
    void setRegionData(juce::Range<double> newRegionData, audium::TimeContextType context) override;
    
    bool validateData();
    
    AudioTrack &getAudioTrack() const { return audioSubGroup.getAudioTrack(); }
    
    double getFileLength(audium::TimeContextType context) const;

    AudioClipData data;
    
private:
    AudioSubGroup &audioSubGroup;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioClip)
    
};
