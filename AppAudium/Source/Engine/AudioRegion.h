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
class AudioSubGroup;

class AudioRegion
{    
    
public:
    AudioRegion(std::shared_ptr<AudioGroup> audioGroup,
                std::shared_ptr<AudioSubGroup> audioSubGroup,
                std::shared_ptr<TempoProvider> tempoProvider) :
        audioGroup(audioGroup),
        audioSubGroup(audioSubGroup),
        tempoProvider(tempoProvider)
    {
        jassert(audioGroup != nullptr);
    }
    
    ~AudioRegion();
    
    std::shared_ptr<AudioGroup> getAudioGroup() const { return audioGroup; }
    std::shared_ptr<AudioSubGroup> getAudioSubGroup() const { return audioSubGroup; }
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;
    
    typedef class juce::Range<double> RegionData;
    
    juce::String name;
    
    void setSelected(bool bSelected) { selected = bSelected; }
    bool isSelected() const { return selected; }
    
    const RegionData getRegionData() const;
    void setRegionData(const RegionData newRegionData);
    
    const RegionData getRegionDataInSeconds() const;
    void setRegionDataInSeconds(const RegionData newRegionData);
    
    bool validateData(RegionData& data);
    
    double getAudioResourceStartInSeconds() const;
    double getAudioResourceEndInSeconds() const;

    void setRegionStart(double newStart);

    void setRegionEnd(double newEnd);

    void setRegionLength(double newLength);
    
private:
    // Start and end position of audio region in seconds.
    RegionData regionData;
    
    std::shared_ptr<AudioGroup> audioGroup;
    
    std::shared_ptr<AudioSubGroup> audioSubGroup;
    
    std::shared_ptr<TempoProvider> tempoProvider;
        
    bool selected = false;
    
    JUCE_LEAK_DETECTOR (AudioRegion)
};
