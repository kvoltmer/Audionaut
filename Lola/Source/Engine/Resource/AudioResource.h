/*
  ==============================================================================

    AudioResource.h
    Created: 29 Jan 2023 12:55:52pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

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
                  int channelPosition);
    
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
    unsigned int getNumChannels() const;
    
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

private:

    AudioResourceContainer& owner;
    
    std::shared_ptr<AudioTrack> audioTrack;
    std::shared_ptr<AudioSubGroup> audioSubGroup;
    
    juce::URL url;
    
    // used for file info
    std::unique_ptr<juce::AudioFormatReader> audioFormatReader;
    
    std::unique_ptr<audium::ChannelMapping> channelMapping;

    double lengthInSeconds = 1.0;
    int numChannels = 1;
    
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResource)
};
