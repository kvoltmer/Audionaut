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

#include <memory>

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "Engine/TimeContext.h"
#include "Engine/Streamable.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Resource/AudioResourceContainer.h"

class AudioPlayer;
class AudiumTransportSource;
class AudioSubGroup;
class AudioRegion;
class AudioChannel;

namespace audium {
    class ChannelMapping;
}

class AudioResource : public audium::Streamable
{
    
public:
    AudioResource(AudioResourceContainer& audioResourceContainer,
                  std::shared_ptr<AudioTrack> audioTrack,
                  std::shared_ptr<AudioSubGroup> audioSubGroup,
                  juce::URL url,
                  std::shared_ptr<juce::AudioFormatReader> reader,
                  int destChannel,
                  int sourceChannel
                  );
    
    virtual ~AudioResource() override;

    std::shared_ptr<AudiumTransportSource> createNewTransportSource(std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource);

    const juce::String getFileNameWithoutExtension() const;
    
    const juce::String getFullPathName() const;
    
    const juce::URL getUrl() const { return url; }
    
    // Returns a string version of the URL.
    const juce::String getUrlAsString() const;
    
    const juce::String getRelativePath(const juce::File &directoryToBeRelativeTo) const;
    
    AudioResourceContainer& getContainer() const { return owner; }
    
    double getSampleRate() const;
    unsigned int getNumAudioFileChannels() const;
    
    double getFileLength(audium::TimeContextType context) const;
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesWithinSubGroup() const;
    
    bool containsAbsolutePosition(double position, audium::TimeContextType context) const;
    
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    int getSizeInUnits() override { return 1; }
    static const juce::URL urlFromJson (json& input);
    static void testUrl (const juce::URL& url);
    
    std::shared_ptr<AudioTrack> getAudioTrack() const { return audioTrack; }
    std::shared_ptr<AudioSubGroup> getAudioSubGroup() const { return audioSubGroup; }
        
    audium::ChannelMapping &getChannelMapping() const { return *channelMapping.get();}

    // shared between resources
    std::shared_ptr<juce::AudioFormatReader> audioFormatReader;
    
private:

    AudioResourceContainer& owner;
    
    std::shared_ptr<AudioTrack> audioTrack;
    std::shared_ptr<AudioSubGroup> audioSubGroup;
    
    juce::URL url;
    
    

    
    std::unique_ptr<audium::ChannelMapping> channelMapping;

    double lengthInSeconds = 1.0;
    int numChannels = 1;
    
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResource)
};
