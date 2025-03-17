//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "Engine/Group/AudioClipData.h"
#include "Engine/Group/AudioSubGroup.h"
#include "Engine/Streamable.h"
#include "Engine/PlayList/PositionableBase.h"

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
