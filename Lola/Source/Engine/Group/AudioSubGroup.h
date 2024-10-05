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

using json = nlohmann::json;

class AudioGroup;
class AudioResource;
class AudioRegion;
class AudioClip;
class AudiumTransportSource;

class AudioSubGroup : public audium::Streamable, public audium::Selectable
{
        
public:
    AudioSubGroup(AudioGroup& audioGroup, std::shared_ptr<audium::SelectionManager> selectionManager);
    virtual ~AudioSubGroup() override;
    void cleanup();
    void cleanupAudioRegions();
    void cleanupAudioResources();
    void cleanupTransportSources();

    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream, bool rebuild) override;
    
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    
    int getSizeInUnits() override;
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;
    
    std::vector<std::shared_ptr<AudioRegion>> getAudioRegions() const;
    
    int getNumChannels() const;
    std::shared_ptr<AudioResource> getChannel(int rowNumber) const;

    AudioGroup& getAudioGroup() const { return audioGroup; }
    
    std::shared_ptr<AudioClip> getAudioClip() const { return audioClip; }
    
    const std::vector<std::shared_ptr<AudiumTransportSource>> &getTransportSources() const { return transportSources; }

private:
    std::shared_ptr<AudioClip> audioClip;
    
    AudioGroup& audioGroup;

    std::vector<std::shared_ptr<AudiumTransportSource>> transportSources;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSubGroup)

};
