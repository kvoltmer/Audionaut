//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

#include "Engine/TimeContext.h"
#include "Engine/Streamable.h"
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/PlayList/PositionableBase.h"

using json = nlohmann::json;

namespace audium {

class AudioTrack;
class AudioResource;
class AudioRegion;
class AudiumTransportSource;
class AudioClip;
class AudioChannel;
class AudioRegionContainer;

class ResourceGroup :   public PositionableBase,
                        public Selectable,
                        public Streamable
{
    
public:
    ResourceGroup(AudioTrack& audioTrack,
                  std::shared_ptr<AudioRegionContainer> audioRegionContainer,
                  std::shared_ptr<SelectionManager> selectionManager);
    virtual ~ResourceGroup() override;
    void cleanup() override;
    void cleanupAudioRegions();
    void cleanupAudioResources();
    void cleanupTransportSources();
    
    // PositionableBase overrides
    double getAbsolutePosition(TimeContextType context) const override;
    void setAbsolutePosition(double position, TimeContextType context) override;
    juce::Range<double> getRegionData(TimeContextType context) const override;
    void setRegionData(juce::Range<double> newRegionData, TimeContextType context) override;
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream, bool rebuild) override;
    
    
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    bool writeChannelToJson (json& output, AudioChannel* audioChannel);
    void mergeFromJson(json& input, int destinationChannel = -1);
    
    
    int getSizeInUnits() override;
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;
    std::shared_ptr<AudioResource> addAudioResourceFromUrl(juce::URL url);
    
    int getNumChannels() const;
    std::shared_ptr<AudioResource> getAudioResourceAtChannel(int channelNumber) const;
    
    AudioTrack& getAudioTrack() const { return audioTrack; }
    
    std::shared_ptr<AudioClip> getAudioClip() const { return audioClip; }
    
    std::shared_ptr<AudioRegionContainer> getAudioRegionContainer() const { return audioRegionContainer; }
    
    const std::vector<std::shared_ptr<AudiumTransportSource>> &getTransportSources() const { return transportSources; }
    
    const juce::String getName() const;
    
    const int getId() const;
    
private:
    std::shared_ptr<AudioClip> audioClip;
    
    AudioTrack& audioTrack;
    
    std::shared_ptr<AudioRegionContainer> audioRegionContainer;
    
    std::vector<std::shared_ptr<AudiumTransportSource>> transportSources;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResourceGroup)
    
};

} // namespace audium
