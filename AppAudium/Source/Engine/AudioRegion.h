/*
  ==============================================================================

    AudioRegion.h
    Created: 30 May 2023 10:16:15am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

class AudioGroup;
class AudioResource;
class TempoProvider;

class AudioRegion
{    
    
public:
    AudioRegion(std::shared_ptr<AudioGroup> audioGroup,
                std::shared_ptr<juce::AudioThumbnailCache> audioThumbnailCache,
                std::shared_ptr<TempoProvider> tempoProvider) :
        audioGroup(audioGroup),
        tempoProvider(tempoProvider)
    {
        jassert(audioGroup != nullptr);
        createThumbnails(audioThumbnailCache);
    }
    
    ~AudioRegion();
    
    void createThumbnails(std::shared_ptr<juce::AudioThumbnailCache> audioThumbnailCache);
    
    juce::AudioThumbnail* getAudioThumbnailForResource(std::shared_ptr<AudioResource> resource) const;
    
    std::shared_ptr<AudioGroup> getAudioGroup() const { return audioGroup; }
    
    typedef class juce::Range<double> RegionData;
    
    juce::String name;
    
    void setSelected(bool bSelected) { selected = bSelected; }
    bool isSelected() const { return selected; }
    
    const RegionData getRegionData() const;
    void setRegionData(const RegionData newRegionData);
    
    const RegionData getRegionDataInSeconds() const;
    void setRegionDataInSeconds(const RegionData newRegionData);
    
    void setRegionStart(double newStart);

    void setRegionEnd(double newEnd);

    void setRegionLength(double newLength);
    
private:
    // Start and end position of audio file in seconds.
    RegionData regionData;
    
    std::shared_ptr<AudioGroup> audioGroup;
    
    typedef std::pair<std::shared_ptr<AudioResource>, std::unique_ptr<juce::AudioThumbnail>> tResourceThumbnailPair;
    
    std::vector<tResourceThumbnailPair> audioThumbnails;
    
    std::shared_ptr<TempoProvider> tempoProvider;
        
    bool selected = false;
    
    JUCE_LEAK_DETECTOR (AudioRegion)
};
