//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <JuceHeader.h>

#include "Engine/TimeContext.h"
#include "Engine/Streamable.h"
#include "Engine/Region/AudioRegionData.h"
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"

namespace audium {

class AudioTrack;
class AudioResource;
class TempoProvider;
class ResourceGroup;

/**
 * @class AudioRegion
 * @brief Represents an audio region within a track.
 *
 * The `AudioRegion` class manages a segment of audio within a track, including its
 * metadata, associated resources, and operations for serialization and editing.
 */
class AudioRegion : public audium::Streamable, public audium::Selectable
{    
    
public:
    /**
     * @brief Constructs an `AudioRegion` object.
     * @param audioTrack A shared pointer to the associated `AudioTrack`.
     * @param resourceGroup A shared pointer to the associated `ResourceGroup`.
     * @param tempoProvider A shared pointer to the `TempoProvider`.
     * @param selectionManager A shared pointer to the `SelectionManager`.
     */
    AudioRegion(std::shared_ptr<AudioTrack> audioTrack_,
                std::shared_ptr<ResourceGroup> resourceGroup_,
                std::shared_ptr<TempoProvider> tempoProvider_,
                std::shared_ptr<SelectionManager> selectionManager_) :
        audium::Selectable(selectionManager_),
        audioTrack(audioTrack_),
        resourceGroup(resourceGroup_),
        tempoProvider(tempoProvider_)
    {
        jassert(audioTrack != nullptr);
    }
    
    /**
      * @brief Destructor for `AudioRegion`.
      */
    ~AudioRegion() override = default;

    /**
    * @brief Sends an action message.
    * @param message The message to send.
    */
    void sendActionMessage(const juce::String& message) const;

    /**
    * @brief Writes the region data to a stream.
    * @param outputStream The output stream to write to.
    * @return True if the operation was successful, false otherwise.
    */
    bool writeToStream(juce::OutputStream& outputStream) override;

    /**
    * @brief Reads the region data from a stream.
    * @param inputStream The input stream to read from.
    * @param rebuild Whether to rebuild the region after reading.
    * @return True if the operation was successful, false otherwise.
    */
    bool readFromStream(juce::InputStream& inputStream, bool rebuild) override;

    /**
    * @brief Serializes the region data to a JSON object.
    * @param output The JSON object to write to.
    * @return True if the operation was successful, false otherwise.
    */
    bool writeToJson(json& output) override;


    /**
    * @brief Retrieves the size of the region in units.
    * @return The size of the region in units.
    */
    int getSizeInUnits() override;

    
    /**
     * @brief Retrieves the associated audio track.
     * @return A shared pointer to the `AudioTrack`.
     */
    std::shared_ptr<AudioTrack> getAudioTrack() const { return audioTrack; }
    
    /**
     * @brief Retrieves the associated resource group.
     * @return A shared pointer to the `ResourceGroup`.
     */
    std::shared_ptr<ResourceGroup> getResourceGroup() const { return resourceGroup; }
    
    /**
     * @brief Retrieves the audio resources associated with the region.
     * @return A vector of shared pointers to `AudioResource` objects.
     */
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;

    /**
     * @brief Retrieves the name of the region.
     * @return The name of the region as a `juce::String`.
     */
    juce::String getName() const { return data.name; }
    
    /**
     * @brief Sets the name of the region.
     * @param newName The new name for the region.
     */
    void setName(const juce::String newName) { data.name = newName.toStdString(); }
    
    /**
     * @brief Retrieves the region data for a specific time context.
     * @param context The time context type.
     * @return The region data as a `tRange`.
     */
    const AudioRegionData::tRange getRegionData(audium::TimeContextType context) const;

    /**
     * @brief Sets the region data for a specific time context.
     * @param newRegionData The new region data.
     * @param context The time context type.
     */
    void setRegionData(const AudioRegionData::tRange newRegionData, audium::TimeContextType context);

    /**
     * @brief Validates the region data for a specific time context.
     * @param data The region data to validate.
     * @param context The time context type.
     * @return True if the data is valid, false otherwise.
     */
    bool validateData(AudioRegionData::tRange& data, audium::TimeContextType context);

    /**
     * @brief Sets the start time of the region in a specific time context.
     * @param newStart The new start time in seconds.
     * @param context The time context type.
     */
    void setRegionStart(double newStart, audium::TimeContextType context);

    /**
     * @brief Sets the end time of the region in a specific time context.
     * @param newEnd The new end time in seconds.
     * @param context The time context type.
     */
    void setRegionEnd(double newEnd, audium::TimeContextType context);

    /**
     * @brief Sets the length of the region in a specific time context.
     * @param newLength The new length in seconds.
     * @param context The time context type.
     */
    void setRegionLength(double newLength, audium::TimeContextType context);

    /**
     * @brief Deletes associated items of the region.
     * @return True if the operation was successful, false otherwise.
     */
    bool deleteAssociatedItems();

    /**
     * @brief Sets the gain for a specific channel.
     * @param channel The channel index.
     * @param newGain The new gain value (linear range).
     * @param continous Whether the gain change is continuous.
     */
    void setGain(int channel, double newGain, bool continous = false);

    /**
     * @brief Retrieves the gain for a specific channel.
     * @param channel The channel index.
     * @return The gain value (linear range).
     */
    double getGain(int channel) const;

    /**
     * @brief Handles the deletion of a specific channel.
     * @param channel The channel index to delete.
     */
    void onDeleteChannel(int channel);
    
    /**
     * @brief Retrieves the maximum sample rate of the audio resources.
     * @return The sample rate.
     */
    double getResourcesMaxSampleRate() const;
    
    /**
     * @brief Retrieves the maximum bit depth of the audio resources.
     * @return The bit depth.
     */
    int getResourcesMaxBitDepth() const;
    
    
    /**
     * @brief Retrieves the recording state.
     * @return The recording state.
     */
    bool isRecording() const;

    /// The region's data.
    AudioRegionData data;

private:
    /// A shared pointer to the associated audio track.
    std::shared_ptr<AudioTrack> audioTrack;

    /// A shared pointer to the associated resource group.
    std::shared_ptr<ResourceGroup> resourceGroup;

    /// A shared pointer to the tempo provider.
    std::shared_ptr<TempoProvider> tempoProvider;

    JUCE_LEAK_DETECTOR(AudioRegion)
};

} // namespace audium
