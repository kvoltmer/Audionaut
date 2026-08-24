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
class VoiceSource;
class AudioClip;
class AudioChannel;
class AudioRegionContainer;

/**
 * @class ResourceGroup
 * @brief Represents a group of audio resources and regions within an audio track.
 *
 * The `ResourceGroup` class manages audio resources, regions, and transport sources
 * associated with an audio track. It provides functionality for serialization,
 * deserialization, and cleanup of resources.
 */
class ResourceGroup : public Selectable,
                      public Streamable
{
public:
    /**
     * @brief Constructs a `ResourceGroup` instance.
     * @param audioTrack Reference to the associated `AudioTrack`.
     * @param audioRegionContainer Shared pointer to the container holding audio regions.
     * @param selectionManager Shared pointer to the selection manager.
     */
    ResourceGroup(AudioTrack& audioTrack,
                  std::shared_ptr<AudioRegionContainer> audioRegionContainer,
                  std::shared_ptr<SelectionManager> selectionManager);

    /**
     * @brief Destructor for `ResourceGroup`.
     */
    virtual ~ResourceGroup() override;

    /**
     * @brief Cleans up resources associated with the resource group.
     */
    void cleanup() override;

    /**
     * @brief Cleans up all audio regions in the resource group.
     */
    void cleanupAudioRegions();

    /**
     * @brief Cleans up all audio resources in the resource group.
     */
    void cleanupAudioResources();

    /**
     * @brief Writes the resource group data to a stream.
     * @param outputStream The output stream to write to.
     * @return True if the operation succeeds, false otherwise.
     */
    bool writeToStream(juce::OutputStream& outputStream) override;

    /**
     * @brief Reads the resource group data from a stream.
     * @param inputStream The input stream to read from.
     * @param rebuild Whether to rebuild the resource group during reading.
     * @return True if the operation succeeds, false otherwise.
     */
    bool readFromStream(juce::InputStream& inputStream, bool rebuild) override;

    /**
     * @brief Writes the resource group data to a JSON object.
     * @param output The JSON object to write to.
     * @return True if the operation succeeds, false otherwise.
     */
    bool writeToJson(json& output) override;

    /**
     * @brief Reads the resource group data from a JSON object.
     * @param input The JSON object to read from.
     * @param rebuild Whether to rebuild the resource group during reading.
     * @return True if the operation succeeds, false otherwise.
     */
    bool readFromJson(json& input, bool rebuild) override;

    /**
     * @brief Writes a specific channel's data to a JSON object.
     * @param output The JSON object to write to.
     * @param audioChannel Pointer to the `AudioChannel` to write.
     * @return True if the operation succeeds, false otherwise.
     */
    bool writeChannelToJson(json& output, AudioChannel* audioChannel);

    /**
     * @brief Merges data from a JSON object into the resource group.
     * @param input The JSON object to merge from.
     * @param destinationChannel The destination channel index (-1 for default).
     */
    void mergeFromJson(json& input, int destinationChannel = -1);

    /**
     * @brief Gets the size of the resource group in units.
     * @return The size of the resource group in units.
     */
    int getSizeInUnits() override;

    /**
     * @brief Gets all audio resources in the resource group.
     * @return A vector of shared pointers to `AudioResource` instances.
     */
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;

    /**
     * @brief Adds a new audio resource from a URL.
     * @param url The URL of the audio file.
     * @return A shared pointer to the created `AudioResource`.
     */
    std::shared_ptr<AudioResource> addAudioResourceFromUrl(juce::URL url);

    /**
     * @brief Gets the number of channels in the resource group.
     * @return The number of channels.
     */
    int getNumChannels() const;

    /**
     * @brief Gets the audio resource at a specific channel.
     * @param channelNumber The channel index.
     * @return A shared pointer to the `AudioResource` at the specified channel.
     */
    std::shared_ptr<AudioResource> getAudioResourceAtChannel(int channelNumber) const;

    /**
     * @brief Gets the associated audio track.
     * @return Reference to the associated `AudioTrack`.
     */
    AudioTrack& getAudioTrack() const { return audioTrack; }
    
    
    std::shared_ptr<AudioRegionContainer> getAudioRegionContainer() const { return audioRegionContainer; }
        
    /**
     * @brief Gets the name of the resource group.
     * @return The name of the resource group.
     */
    const juce::String getName() const;

    /**
     * @brief Gets the ID of the resource group.
     * @return The ID of the resource group.
     */
    const int getId() const;
    
    
    double getMaxLength(audium::TimeContextType context) const;

private:
    AudioTrack& audioTrack; ///< Reference to the associated audio track.
    std::shared_ptr<AudioRegionContainer> audioRegionContainer; ///< Shared pointer to the audio region container.
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResourceGroup)
    
};

} // namespace audium
