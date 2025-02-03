/*
  ==============================================================================

    AudioSubGroup.h
    Created: 19 Dec 2023 3:47:13pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

#include "Engine/TimeContext.h"
#include "Engine/Streamable.h"
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/PlayList/PositionableBase.h"

using json = nlohmann::json;

class AudioTrack;
class AudioResource;
class AudioRegion;
class AudiumTransportSource;
class AudioClip;
class AudioChannel;

class AudioSubGroup :   public PositionableBase,
                        public audium::Selectable,
                        public audium::Streamable
{
        
public:
    AudioSubGroup(AudioTrack& audioTrack, std::shared_ptr<audium::SelectionManager> selectionManager);
    virtual ~AudioSubGroup() override;
    void cleanup() override;
    void cleanupAudioRegions();
    void cleanupAudioResources();
    void cleanupTransportSources();

    // PositionableBase overrides
    double getAbsolutePosition(audium::TimeContextType context) const override;
    void setAbsolutePosition(double position, audium::TimeContextType context) override;
    juce::Range<double> getRegionData(audium::TimeContextType context) const override;
    void setRegionData(juce::Range<double> newRegionData, audium::TimeContextType context) override;
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream, bool rebuild) override;
    
    
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    bool writeChannelToJson (json& output, AudioChannel* audioChannel);
    void mergeFromJson(json& input, int destinationChannel = -1);

    
    int getSizeInUnits() override;
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;
    std::shared_ptr<AudioResource> addAudioResourceFromUrl(const juce::URL url);
    
    std::vector<std::shared_ptr<AudioRegion>> getAudioRegions() const;
    
    int getNumChannels() const;
    std::shared_ptr<AudioResource> getChannel(int rowNumber) const;

    AudioTrack& getAudioTrack() const { return audioTrack; }
    
    std::shared_ptr<AudioClip> getAudioClip() const { return audioClip; }
    
    const std::vector<std::shared_ptr<AudiumTransportSource>> &getTransportSources() const { return transportSources; }

    const juce::String getName() const;
    
private:
    std::shared_ptr<AudioClip> audioClip;
    
    AudioTrack& audioTrack;

    std::vector<std::shared_ptr<AudiumTransportSource>> transportSources;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSubGroup)

};
