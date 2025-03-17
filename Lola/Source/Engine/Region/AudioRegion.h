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
#include <JuceHeader.h>

#include "Engine/TimeContext.h"
#include "Engine/Streamable.h"
#include "Engine/Region/AudioRegionData.h"
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"

namespace audium {

class AudioTrack;
class AudioResource;
class TempoProvider;
class AudioSubGroup;

class AudioRegion : public audium::Streamable, public audium::Selectable
{    
    
public:
    AudioRegion(std::shared_ptr<AudioTrack> audioTrack,
                std::shared_ptr<AudioSubGroup> audioSubGroup,
                std::shared_ptr<TempoProvider> tempoProvider,
                std::shared_ptr<SelectionManager> selectionManager) :
    audium::Selectable(selectionManager),
    audioTrack(audioTrack),
    audioSubGroup(audioSubGroup),
    tempoProvider(tempoProvider)
    {
        jassert(audioTrack != nullptr);
    }
    
    ~AudioRegion() = default;
    
    void sendActionMessage (const juce::String& message) const;
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream, bool rebuild) override;
    
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    
    int getSizeInUnits() override;
    
    std::shared_ptr<AudioTrack> getAudioTrack() const { return audioTrack; }
    std::shared_ptr<AudioSubGroup> getAudioSubGroup() const { return audioSubGroup; }
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;
    
    juce::String getName() const { return data.name; }
    void setName(const juce::String newName) { data.name = newName.toStdString(); }
    
    const AudioRegionData::tRange getRegionData(audium::TimeContextType context) const;
    void setRegionData(const AudioRegionData::tRange newRegionData, audium::TimeContextType context);
    
    bool validateData(AudioRegionData::tRange& data, audium::TimeContextType context);
    
    double getAudioResourceStart(audium::TimeContextType context) const;
    double getAudioResourceEnd(audium::TimeContextType context) const;
    
    void setRegionStart(double newStart, audium::TimeContextType context);
    void setRegionEnd(double newEnd, audium::TimeContextType context);
    void setRegionLength(double newLength, audium::TimeContextType context);
    
    bool deleteAssociatedItems();
    
    // gain [linear value range]
    void setGain(int channel, double newGain, bool continous = false);
    double getGain(int channel) const;
    
    void onDeleteChannel(int channel);
    
    AudioRegionData data;
    
private:
    
    std::shared_ptr<AudioTrack> audioTrack;
    std::shared_ptr<AudioSubGroup> audioSubGroup;
    std::shared_ptr<TempoProvider> tempoProvider;
    
    JUCE_LEAK_DETECTOR (AudioRegion)
};

} // namespace audium
