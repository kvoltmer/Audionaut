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
class AudioGroup;
class AudioSubGroup;
class AudioRegion;

class AudioResource {
    
public:
    AudioResource(AudioResourceContainer& audioResourceContainer,
                  std::shared_ptr<AudioGroup> audioGroup,
                  std::shared_ptr<AudioSubGroup> audioSubGroup,
                  juce::URL url,
                  std::shared_ptr<AudiumTransportSource> transportSource,
                  std::unique_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource,
                  int channelPosition,
                  int resourceId) :
        owner(audioResourceContainer),
        audioGroup(audioGroup),
        audioSubGroup(audioSubGroup),
        url(url),
        transportSource(transportSource),
        channelPosition(channelPosition),
        resourceId(resourceId)
    {
        this->audioFormatReaderSource = std::move(audioFormatReaderSource);
        setRegionDataInSeconds(juce::Range<double>(0.0, getLengthInSeconds()), false);
    }
    
    ~AudioResource();

    std::shared_ptr<AudiumTransportSource> getAudioTransportSource() const { return transportSource; }

    const juce::String getFileNameWithoutExtension() const;
    
    const juce::String getFullPathName() const;
    
    const juce::URL getUrl() const { return url; }
    
    // Returns a string version of the URL.
    const juce::String getUrlAsString() const;
    
    /// TODO: discuss moving this to GroupListBoxModel
    int getTop() const { return channelPosition * height; }
    int getHeight() const { return height * getNumChannels(); }
    int getChannelHeight() const { return height; }
    void setChannelHeight(int newHeight) { height = newHeight; }
    
    AudioResourceContainer& getContainer() const { return owner; }
    
    double getSampleRate() const;
    unsigned int getNumChannels() const;
    double getLengthInSeconds() const;
    
    double getAbsolueStartTime() const;
    double getDurationTimeInSeconds() const;
    const juce::Range<double> getRegionDataInSeconds() const;
    
    void setRegionDataInSeconds(const juce::Range<double> newRegionData, bool syncEqualResources);
    void setTransportPosition(const double newPosition, bool syncEqualResources);
    bool validateData(bool syncResources);
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesWithinSubGroup() const;
    
    double getTransportPositionSeconds() const;
    double getTransportPositionClocks() const { return transportPositionClocks; }
    bool containsAbsolutePosition(double position) const;
        
    int getChannelPosition() const { return channelPosition; }
    
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);
    
    std::shared_ptr<AudioGroup> getAudioGroup() const { return audioGroup; }
    std::shared_ptr<AudioSubGroup> getAudioSubGroup() const { return audioSubGroup; }
    
    void setSelected(bool bSelected, bool deselectOthers);
    bool isSelected() const { return selected; }
        
    const int getId() const noexcept { return resourceId; }
    void setId(const int newId) { resourceId = newId; }
    
private:

    AudioResourceContainer& owner;
    
    std::shared_ptr<AudioGroup> audioGroup;
    std::shared_ptr<AudioSubGroup> audioSubGroup;
    
    juce::URL url;
    
    std::shared_ptr<AudiumTransportSource> transportSource;
    
    std::unique_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource;
    
    
    /// TODO: capsulate the data below
    juce::Range<double> regionData;
    double transportPositionClocks = 0.0;
    int channelPosition = 0;
    int height = 100;
    bool selected = false;
    int resourceId = -1;
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResource)
};
