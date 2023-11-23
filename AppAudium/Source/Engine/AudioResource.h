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
                  std::shared_ptr<AudiumTransportSource> transportSource,
                  std::unique_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource) :
        owner(audioResourceContainer),
        url(url),
        transportSource(transportSource)
    {
        this->audioFormatReaderSource = std::move(audioFormatReaderSource);
    }
    
    ~AudioResource();

    std::shared_ptr<AudiumTransportSource> getAudioTransportSource() const { return transportSource; }

    const juce::String getFileNameWithoutExtension() const;
    
    const juce::String getFullPathName() const;
    
    const juce::URL getUrl() const { return url; }
    
    // Returns a string version of the URL.
    const juce::String getUrlAsString() const;
    
    /// TODO: move this to AudioGroupListBoxModel
    int getHeight() const { return height * getNumChannels(); }
    int getChannelHeight() const { return height; }
    
    AudioResourceContainer& getContainer() const { return owner; }
    
    double getSampleRate() const;
    unsigned int getNumChannels() const;
    double getLengthInSeconds() const;
    
private:

    AudioResourceContainer& owner;
    
    juce::URL url;
    
    std::shared_ptr<AudiumTransportSource> transportSource;
    
    std::unique_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource;
    
    int height = 100;
    
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResource)
};
