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

class AudioResourceContainer;
class AudioPlayer;
class AudiumTransportSource;
class AudioGroup;
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
                  std::shared_ptr<AudiumTransportSource> transportSource,
                  std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource,
                  int channelPosition,
                  int resourceId) :
        owner(audioResourceContainer),
        audioGroup(audioGroup),
        audioSubGroup(audioSubGroup),
        url(url),
        transportSource(transportSource),
        audioFormatReaderSource(audioFormatReaderSource),
        resourceId(resourceId)
    {
        setRegionData(juce::Range<double>(0.0, getFileLength(audium::seconds)), audium::seconds);
        setChannelPosition(channelPosition);
    }
    
    virtual ~AudioResource();

    std::shared_ptr<AudiumTransportSource> getAudioTransportSource() const { return transportSource; }
    
    juce::AudioFormatReader* getAudioFormatReader() const { return audioFormatReaderSource->getAudioFormatReader(); }

    const juce::String getFileNameWithoutExtension() const;
    
    const juce::String getFullPathName() const;
    
    const juce::URL getUrl() const { return url; }
    
    // Returns a string version of the URL.
    const juce::String getUrlAsString() const;
    
    AudioResourceContainer& getContainer() const { return owner; }
    
    double getSampleRate() const;
    unsigned int getNumChannels() const;
    
    double getFileLength(audium::TimeContextType context) const;
    
    const juce::Range<double> getRegionData(audium::TimeContextType context) const;
    void setRegionData(const juce::Range<double> newRegionData, audium::TimeContextType context);
    
    bool validateData();
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesWithinSubGroup() const;
    
    bool containsAbsolutePosition(double position, audium::TimeContextType context) const;
            
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream) override;
    int getSizeInUnits() override { return 1; };

    std::shared_ptr<AudioGroup> getAudioGroup() const { return audioGroup; }
    std::shared_ptr<AudioSubGroup> getAudioSubGroup() const { return audioSubGroup; }
    
    void setSelected(bool bSelected, bool deselectOthers);
    bool isSelected() const { return selected; }
        
    const int getId() const noexcept { return resourceId; }
    void setId(const int newId) { resourceId = newId; }
    
    bool containsChannelNumber(int channelNumber) const;
    int getChannelPosition() const;
    void setChannelPosition(int startChannel);
    bool deleteChannel(std::shared_ptr<AudioChannel> channel);
    
private:

    AudioResourceContainer& owner;
    
    std::shared_ptr<AudioGroup> audioGroup;
    std::shared_ptr<AudioSubGroup> audioSubGroup;
    std::vector<std::shared_ptr<AudioChannel>> audioChannels;
    
    juce::URL url;
    
    std::shared_ptr<AudiumTransportSource> transportSource;
            
    std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource;
    
    
    /// TODO: capsulate the data below
    juce::Range<double> regionData;
    
    bool selected = false;
    int resourceId = -1;
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResource)
};
