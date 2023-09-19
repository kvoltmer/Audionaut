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
    
    void start();
    
    void stop();

    std::shared_ptr<juce::AudioTransportSource> getAudioTransportSource();
    
    bool isThumbnailFullyLoaded() const { return thumbnail.isFullyLoaded(); }

    const juce::String getFileNameWithoutExtension() const;
    
    const juce::String getFullPathName() const;
    
    /// TODO: move this to WaveFormTableListBoxModel
    int height = 100;
    
    /// TODO: move this to a gui state
    juce::Colour currentColour;
    
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);
    
    AudioResourceContainer& getContainer() const { return owner; }
    
    double getSampleRate() const;
    
private:

    AudioResourceContainer& owner;
    
    juce::URL url;
    
    juce::AudioThumbnail thumbnail;
    
    std::shared_ptr<AudioPlayer> audioPlayer;
    
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResource)
};
