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
#include "Engine/Streamable.h"
#include "Engine/Region/AudioRegionData.h"
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"

class AudioTrack;
class AudioResource;
class TempoProvider;
class AudioSubGroup;

//==============================================================================
class AudioRegion : public audium::Streamable, public audium::Selectable
{    
    
public:
    AudioRegion(std::shared_ptr<AudioTrack> audioTrack,
                std::shared_ptr<AudioSubGroup> audioSubGroup,
                std::shared_ptr<TempoProvider> tempoProvider,
                std::shared_ptr<audium::SelectionManager> selectionManager) :
        audium::Selectable(selectionManager),
        audioTrack(audioTrack),
        audioSubGroup(audioSubGroup),
        tempoProvider(tempoProvider)
    {
        jassert(audioTrack != nullptr);
    }
    
    ~AudioRegion();
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream, bool rebuild) override;
    
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    
    int getSizeInUnits() override;
    
    std::shared_ptr<AudioTrack> getAudioTrack() const { return audioTrack; }
    std::shared_ptr<AudioSubGroup> getAudioSubGroup() const { return audioSubGroup; }
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;
    
    juce::String getName() const { return data.name; }
    void setName(const juce::String newName) { data.name = newName.toStdString(); }
    
    const AudioRegionData::tRange getRegionData(audium::TimeContextType context) const;
    void setRegionData(const AudioRegionData::tRange newRegionData, audium::TimeContextType context);
    
    bool validateData(AudioRegionData::tRange& data, audium::TimeContextType context);
    
    double getAudioResourceStart(audium::TimeContextType context) const;
    double getAudioResourceEnd(audium::TimeContextType context) const;
    
    void setRegionStart(double newStart, audium::TimeContextType context);
    void setRegionEnd(double newEnd, audium::TimeContextType context);
    void setRegionLength(double newLength, audium::TimeContextType context);
    
    bool deleteAssociatedItems();
    
    AudioRegionData data;
    
private:
    
    std::shared_ptr<AudioTrack> audioTrack;
    std::shared_ptr<AudioSubGroup> audioSubGroup;
    std::shared_ptr<TempoProvider> tempoProvider;
    
    JUCE_LEAK_DETECTOR (AudioRegion)
};

