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
                  std::unique_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource,
                  std::shared_ptr<juce::AudioThumbnail> audioThumbnail,
                  int channelPosition) :
        owner(audioResourceContainer),
        url(url),
        transportSource(transportSource),
        audioThumbnail(audioThumbnail),
        channelPosition(channelPosition)
    {
        this->audioFormatReaderSource = std::move(audioFormatReaderSource);
        setRegionDataInSeconds(juce::Range<double>(0.0, getLengthInSeconds()));
    }
    
    ~AudioResource();

    std::shared_ptr<AudiumTransportSource> getAudioTransportSource() const { return transportSource; }

    const juce::String getFileNameWithoutExtension() const;
    
    const juce::String getFullPathName() const;
    
    const juce::URL getUrl() const { return url; }
    
    // Returns a string version of the URL.
    const juce::String getUrlAsString() const;
    
    /// TODO: discuss moving this to AudioGroupListBoxModel
    int getTop() const { return channelPosition * height; }
    int getHeight() const { return height * getNumChannels(); }
    int getChannelHeight() const { return height; }
    
    AudioResourceContainer& getContainer() const { return owner; }
    
    double getSampleRate() const;
    unsigned int getNumChannels() const;
    double getLengthInSeconds() const;
    
    double getAbsolueStartTime() const;
    double getDurationTimeInSeconds() const;
    const juce::Range<double> getRegionDataInSeconds() const;
    void setRegionDataInSeconds(const juce::Range<double> newRegionData);
    void setTransportPosition(const double newPosition);
    double getTransportPosition() const;
    bool containsAbsolutePosition(double position) const;
    
    juce::AudioThumbnail* getAudioThumbnail() const { return audioThumbnail.get(); }
    
    int getChannelPosition() const { return channelPosition; }
    
private:

    AudioResourceContainer& owner;
    
    juce::URL url;
    
    std::shared_ptr<AudiumTransportSource> transportSource;
    
    std::unique_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource;
    
    std::shared_ptr<juce::AudioThumbnail> audioThumbnail;
    
    juce::Range<double> regionData;
    
    double transportPositionClocks = 0.0;
    
    int channelPosition = 0;
    
    int height = 100;
    
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResource)
};
