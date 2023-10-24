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

class AudioResourceContainer;
class AudioPlayer;
class AudiumTransportSource;

class AudioResource {
    
public:
    AudioResource(AudioResourceContainer& audioResourceContainer,
                  juce::URL url,
                  juce::InputSource* inputSource,
                  juce::AudioFormatManager& formatManager,
                  std::shared_ptr<AudioPlayer> audioPlayer,
                  juce::AudioThumbnailCache& thumbnailCache);
    ~AudioResource();
    
    juce::AudioThumbnail& getThumbnail() { return thumbnail; }
    
    /// returns the maximum length of all audio resources
    double getTotalLengthMax() const;

    std::shared_ptr<AudiumTransportSource> getAudioTransportSource();
    
    bool isThumbnailFullyLoaded() const { return thumbnail.isFullyLoaded(); }

    const juce::String getFileNameWithoutExtension() const;
    
    const juce::String getFullPathName() const;
    
    // Returns a string version of the URL.
    const juce::String getUrlAsString() const;
    
    /// TODO: move this to AudioGroupListBoxModel
    int getHeight() const { return height * getNumChannels(); }
    int getChannelHeight() const { return height; }
    
    
    
    AudioResourceContainer& getContainer() const { return owner; }
    std::shared_ptr<AudioPlayer> getAudioPlayer() const { return audioPlayer; }
    
    double getSampleRate() const;
    unsigned int getNumChannels() const;

private:

    AudioResourceContainer& owner;
    
    juce::URL url;
    
    juce::AudioThumbnail thumbnail;
    
    std::shared_ptr<AudioPlayer> audioPlayer;
    
    int height = 100;
    
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResource)
};
