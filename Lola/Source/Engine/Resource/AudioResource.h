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
#include "Engine/Group/AudioGroup.h"
#include "Engine/Resource/AudioResourceContainer.h"

class AudioPlayer;
class AudiumTransportSource;
class AudioSubGroup;
class AudioRegion;
class AudioChannel;

class AudioResource : public audium::Streamable
{
    
public:
    AudioResource(AudioResourceContainer& audioResourceContainer,
                  std::shared_ptr<AudioGroup> audioGroup,
                  std::shared_ptr<AudioSubGroup> audioSubGroup,
                  juce::URL url,
                  int channelPosition);
    
    virtual ~AudioResource();

    std::shared_ptr<AudiumTransportSource> createNewTransportSource(juce::TimeSliceThread* readAheadThread,
                                                                    std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource);

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
    int getSizeInUnits() override { return 1; };
    static const juce::URL urlFromJson (json& input);

    std::shared_ptr<AudioGroup> getAudioGroup() const { return audioGroup; }
    std::shared_ptr<AudioSubGroup> getAudioSubGroup() const { return audioSubGroup; }
    
    void setSelected(bool bSelected, bool deselectOthers);
    bool isSelected() const { return selected; }
        
    bool containsChannelNumber(int channelNumber) const;
    bool containsChannel(std::shared_ptr<AudioChannel> channel) const;
    int getChannelPosition() const;
    void setChannelPosition(int startChannel);
    bool deleteChannel(AudioChannel* channel);
    
private:

    AudioResourceContainer& owner;
    
    std::shared_ptr<AudioGroup> audioGroup;
    std::shared_ptr<AudioSubGroup> audioSubGroup;
    std::vector<std::shared_ptr<AudioChannel>> audioChannels;
    
    juce::URL url;
    
    // used for file info
    std::unique_ptr<juce::AudioFormatReader> audioFormatReader;
    
    bool selected = false;

    double lengthInSeconds = 1.0;
    int numChannels = 1;
    
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResource)
};
