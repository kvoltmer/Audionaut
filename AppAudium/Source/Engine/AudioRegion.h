/*
  ==============================================================================

    AudioRegion.h
    Created: 30 May 2023 10:16:15am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

#include "Engine/TimeContext.h"

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
    
    juce::String getName() const { return name; }
    void setName(const juce::String newName) { name = newName; }
    
    void setSelected(bool bSelected) { selected = bSelected; }
    bool isSelected() const { return selected; }
    
    const RegionData getRegionData(audium::TimeContextType context) const;
    void setRegionData(const RegionData newRegionData, audium::TimeContextType context);
    
    bool validateData(RegionData& data);
    
    double getAudioResourceStartInSeconds() const;
    double getAudioResourceEndInSeconds() const;

    void setRegionStart(double newStart, audium::TimeContextType context);
    void setRegionEnd(double newEnd, audium::TimeContextType context);
    void setRegionLength(double newLength, audium::TimeContextType context);
    
private:
    
    std::shared_ptr<AudioGroup> audioGroup;
    std::shared_ptr<AudioSubGroup> audioSubGroup;
    std::shared_ptr<TempoProvider> tempoProvider;
    
    // Start and end position of audio region in seconds.
    RegionData regionData;

    juce::String name;
    
    bool selected = false;
    
    JUCE_LEAK_DETECTOR (AudioRegion)
};
