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

class AudioRegion
{    
    
public:
    AudioRegion(std::shared_ptr<AudioGroup> audioGroup,
                std::shared_ptr<juce::AudioThumbnailCache> audioThumbnailCache) :
        audioGroup(audioGroup)
    {
        jassert(audioGroup != nullptr);
        createThumbnails(audioThumbnailCache);
    }
    
    ~AudioRegion();
    
    void createThumbnails(std::shared_ptr<juce::AudioThumbnailCache> audioThumbnailCache);
    
    juce::AudioThumbnail* getAudioThumbnailForResource(std::shared_ptr<AudioResource> resource) const;
    
    std::shared_ptr<AudioGroup> getAudioGroup() const { return audioGroup; }
    
    typedef class juce::Range<double> RegionData;
    
    RegionData position;
    
    juce::String name;
    
    void setSelected(bool bSelected) { selected = bSelected; }
    bool isSelected() const { return selected; }
    
private:
    
    std::shared_ptr<AudioGroup> audioGroup;
    
    typedef std::pair<std::shared_ptr<AudioResource>, std::unique_ptr<juce::AudioThumbnail>> tResourceThumbnailPair;
    
    std::vector<tResourceThumbnailPair> audioThumbnails;
        
    bool selected = false;
    
    JUCE_LEAK_DETECTOR (AudioRegion)
};
