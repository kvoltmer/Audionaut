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
#include <nlohmann/json.hpp>

#include "Engine/TimeContext.h"
#include "Engine/Streamable.h"
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/PlayList/PositionableBase.h"

using json = nlohmann::json;

class AudioTrack;
class AudioResource;
class AudioRegion;
class AudiumTransportSource;
class AudioClip;
class AudioChannel;
class AudioRegionContainer;

class AudioSubGroup :   public PositionableBase,
                        public audium::Selectable,
                        public audium::Streamable
{
        
public:
    AudioSubGroup(AudioTrack& audioTrack,
                  std::shared_ptr<AudioRegionContainer> audioRegionContainer,
                  std::shared_ptr<audium::SelectionManager> selectionManager);
    virtual ~AudioSubGroup() override;
    void cleanup() override;
    void cleanupAudioRegions();
    void cleanupAudioResources();
    void cleanupTransportSources();

    // PositionableBase overrides
    double getAbsolutePosition(audium::TimeContextType context) const override;
    void setAbsolutePosition(double position, audium::TimeContextType context) override;
    juce::Range<double> getRegionData(audium::TimeContextType context) const override;
    void setRegionData(juce::Range<double> newRegionData, audium::TimeContextType context) override;
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream, bool rebuild) override;
    
    
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    bool writeChannelToJson (json& output, AudioChannel* audioChannel);
    void mergeFromJson(json& input, int destinationChannel = -1);

    
    int getSizeInUnits() override;
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;
    std::shared_ptr<AudioResource> addAudioResourceFromUrl(const juce::URL url);
        
    int getNumChannels() const;
    std::shared_ptr<AudioResource> getAudioResourceAtChannel(int channelNumber) const;

    AudioTrack& getAudioTrack() const { return audioTrack; }
    
    std::shared_ptr<AudioClip> getAudioClip() const { return audioClip; }
    
    std::shared_ptr<AudioRegionContainer> getAudioRegionContainer() const { return audioRegionContainer; }

    const std::vector<std::shared_ptr<AudiumTransportSource>> &getTransportSources() const { return transportSources; }

    const juce::String getName() const;
    
    const int getId() const;
    
private:
    std::shared_ptr<AudioClip> audioClip;
    
    AudioTrack& audioTrack;
    
    std::shared_ptr<AudioRegionContainer> audioRegionContainer;

    std::vector<std::shared_ptr<AudiumTransportSource>> transportSources;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSubGroup)

};
