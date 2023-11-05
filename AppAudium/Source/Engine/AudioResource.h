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
                  juce::AudioThumbnailCache& thumbnailCache,
                  std::shared_ptr<AudiumTransportSource> transportSource,
                  std::unique_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource) :
        owner(audioResourceContainer),
        url(url),
        thumbnail (4096, formatManager, thumbnailCache),
        transportSource(transportSource)
    {
        this->audioFormatReaderSource = std::move(audioFormatReaderSource);
        thumbnail.setSource(inputSource);
    }
    
    ~AudioResource();
    
    juce::AudioThumbnail& getThumbnail() { return thumbnail; }

    std::shared_ptr<AudiumTransportSource> getAudioTransportSource() const { return transportSource; }

    bool isThumbnailFullyLoaded() const { return thumbnail.isFullyLoaded(); }

    const juce::String getFileNameWithoutExtension() const;
    
    const juce::String getFullPathName() const;
    
    // Returns a string version of the URL.
    const juce::String getUrlAsString() const;
    
    /// TODO: move this to AudioGroupListBoxModel
    int getHeight() const { return height * getNumChannels(); }
    int getChannelHeight() const { return height; }
    
    AudioResourceContainer& getContainer() const { return owner; }
    
    double getSampleRate() const;
    unsigned int getNumChannels() const;

private:

    AudioResourceContainer& owner;
    
    juce::URL url;
    
    juce::AudioThumbnail thumbnail;
    
    std::shared_ptr<AudiumTransportSource> transportSource;
    
    std::unique_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource;
    
    int height = 100;
    
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResource)
};
