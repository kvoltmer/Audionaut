//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Engine/Group/AudioClipData.h"
#include "Engine/Group/AudioSubGroup.h"
#include "Engine/Streamable.h"
#include "Engine/PlayList/PositionableBase.h"

namespace audium {

class AudioClip : public audium::Streamable
{
    
public:
    AudioClip(AudioSubGroup &audioSubGroup) :
    audioSubGroup(audioSubGroup)
    {
    }
    
    double getAbsolutePosition(audium::TimeContextType context) const;
    void setAbsolutePosition(double position, audium::TimeContextType context);
    
    juce::Range<double> getRegionData(audium::TimeContextType context) const;
    void setRegionData(juce::Range<double> newRegionData, audium::TimeContextType context);
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream, bool rebuild) override;
    
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    
    int getSizeInUnits() override { return 1; }
    
    bool validateData();
    
    AudioTrack &getAudioTrack() const;
    
    double getFileLength(audium::TimeContextType context) const;
    
    AudioClipData data;
    
private:
    AudioSubGroup &audioSubGroup;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioClip)
    
};

} // namespace audium
