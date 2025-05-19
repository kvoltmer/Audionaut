//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <JuceHeader.h>
#include "Engine/Region/AudioRegion.h"
#include "Engine/ActionMessages.h"

namespace audium {

class AudioResourceContainer;
class AudioTrackContainer;
class AudioTrack;
class ResourceGroup;
class AudioChannel;

/**
    * @class AudioRegionContainer
    * @brief Manages a collection of audio regions within a track.
    *
    * The `AudioRegionContainer` class provides functionality to create, retrieve, delete,
    * and manage audio regions associated with a specific audio track.
 */
class AudioRegionContainer : public audium::Streamable
{
public:
    /**
     * @brief Constructs an `AudioRegionContainer` for the specified audio track.
     * @param audioTrack_ A reference to the associated `AudioTrack`.
     */
    AudioRegionContainer(AudioTrack &audioTrack_);

    /**
     * @brief Creates a default audio region for the specified track.
     * @param track A shared pointer to the `AudioTrack`.
     * @return A shared pointer to the created `AudioRegion`.
     */
    std::shared_ptr<AudioRegion> createDefaultRegion(std::shared_ptr<AudioTrack> track);

    /**
     * @brief Creates an audio region for the specified track and resource group.
     * @param track A shared pointer to the `AudioTrack`.
     * @param resourceGroup A shared pointer to the `ResourceGroup`.
     * @return A shared pointer to the created `AudioRegion`.
     */
    std::shared_ptr<AudioRegion> createRegion(std::shared_ptr<AudioTrack> track,
                                              std::shared_ptr<ResourceGroup> resourceGroup);

    /**
     * @brief Creates an audio region with the specified parameters.
     * @param regionName The name of the region.
     * @param position The time range of the region.
     * @param track A shared pointer to the `AudioTrack`.
     * @param resourceGroup A shared pointer to the `ResourceGroup`.
     * @param otherRegion A shared pointer to another `AudioRegion` for reference.
     * @param context The time context type.
     * @return A shared pointer to the created `AudioRegion`.
     */
    std::shared_ptr<AudioRegion> createRegion(juce::String regionName,
                                              juce::Range<double> position,
                                              std::shared_ptr<AudioTrack> track,
                                              std::shared_ptr<ResourceGroup> resourceGroup,
                                              std::shared_ptr<AudioRegion> otherRegion,
                                              audium::TimeContextType context);

    /**
     * @brief Creates an audio region based on another region.
     * @param track A shared pointer to the `AudioTrack`.
     * @param resourceGroup A shared pointer to the `ResourceGroup`.
     * @param otherRegion A shared pointer to another `AudioRegion` for reference.
     * @return A shared pointer to the created `AudioRegion`.
     */
    std::shared_ptr<AudioRegion> createRegion(std::shared_ptr<AudioTrack> track,
                                              std::shared_ptr<ResourceGroup> resourceGroup,
                                              const std::shared_ptr<AudioRegion> otherRegion);

    /**
     * @brief Formats a number as a string.
     * @param num The number to format.
     * @return The formatted string.
     */
    static std::string formatNumber(long num);

    /**
     * @brief Retrieves a unique name for a region.
     * @param regionName The base name of the region.
     * @return A unique name as a `juce::String`.
     */
    const juce::String getUniqueName(juce::String regionName) const;

    /**
     * @brief Cleans up unused regions.
     */
    void cleanup();

    /**
     * @brief Retrieves the number of audio regions.
     * @return The number of audio regions.
     */
    int getNumRegions() const;

    /**
     * @brief Retrieves an audio region by index.
     * @param index The index of the region.
     * @return A shared pointer to the `AudioRegion`.
     */
    std::shared_ptr<AudioRegion> getRegion(int index) const;

    /**
     * @brief Retrieves the ID of a specific audio region.
     * @param searchRegion A shared pointer to the `AudioRegion` to search for.
     * @return The ID of the region.
     */
    int getRegionId(std::shared_ptr<AudioRegion> searchRegion) const;

    /**
     * @brief Retrieves regions associated with a specific resource group.
     * @param resourceGroup A pointer to the `ResourceGroup`.
     * @return A vector of shared pointers to `AudioRegion` objects.
     */
    std::vector<std::shared_ptr<AudioRegion>> getRegionsForSubGroup(const ResourceGroup* resourceGroup) const;

    /**
     * @brief Retrieves selected regions.
     * @param global Whether to retrieve globally selected regions.
     * @return A vector of shared pointers to `AudioRegion` objects.
     */
    std::vector<std::shared_ptr<AudioRegion>> getSelectedRegions(bool global = false) const;

    /**
     * @brief Finds a region similar to the specified one.
     * @param otherRegion A shared pointer to the `AudioRegion` to compare.
     * @return A shared pointer to the similar `AudioRegion`, or nullptr if not found.
     */
    std::shared_ptr<AudioRegion> findSimilarRegion(std::shared_ptr<AudioRegion> otherRegion) const;

    /**
     * @brief Deletes a specific audio region.
     * @param region A shared pointer to the `AudioRegion` to delete.
     */
    void deleteAudioRegion(std::shared_ptr<AudioRegion> region);

    /**
     * @brief Deletes a specific audio region by pointer.
     * @param region A pointer to the `AudioRegion` to delete.
     * @return True if the region was successfully deleted, false otherwise.
     */
    bool deleteAudioRegion(AudioRegion* region);

    /**
     * @brief Deletes all regions associated with a specific resource group.
     * @param resourceGroup A shared pointer to the `ResourceGroup`.
     */
    void deleteAudioRegionsForSubGroup(std::shared_ptr<ResourceGroup> resourceGroup);

    /**
     * @brief Deletes unused regions.
     */
    void deleteUnusedRegions();

    /**
     * @brief Sorts the IDs of all regions.
     */
    void sortRegionIds();

    /**
     * @brief Retrieves regions associated with a specific audio resource.
     * @param audioResource A shared pointer to the `AudioResource`.
     * @return A vector of shared pointers to `AudioRegion` objects.
     */
    std::vector<std::shared_ptr<AudioRegion>> getRegionsForResource(std::shared_ptr<AudioResource> audioResource) const;

    /**
     * @brief Retrieves the selected rows in the container.
     * @return A `juce::SparseSet` of selected row indices.
     */
    juce::SparseSet<int> getSelectedRows() const;

    /**
     * @brief Sets the selected rows in the container.
     * @param selectedRows A `juce::SparseSet` of row indices to select.
     */
    void setSelectedRows(juce::SparseSet<int>& selectedRows);

    /**
     * @brief Retrieves a region with specific data.
     * @param data The `AudioRegionData` to search for.
     * @return A shared pointer to the `AudioRegion`, or nullptr if not found.
     */
    std::shared_ptr<AudioRegion> getRegionWithData(const AudioRegionData &data) const;

    /**
     * @brief Retrieves the undo manager.
     * @return A shared pointer to the `juce::UndoManager`.
     */
    std::shared_ptr<juce::UndoManager> getUndoManager() const { return undoManager; }

    /**
     * @brief Retrieves the associated audio track container.
     * @return A reference to the `AudioTrackContainer`.
     */
    AudioTrackContainer& getAudioTrackContainer() const { return audioTrackContainer; }

    /**
     * @brief Retrieves the associated audio resource container.
     * @return A reference to the `AudioResourceContainer`.
     */
    AudioResourceContainer& getAudioResourceContainer() const { return audioResourceContainer; }

    /**
     * @brief Serializes the container to a JSON object.
     * @param output The JSON object to write to.
     * @return True if the operation was successful, false otherwise.
     */
    bool writeToJson(json& output) override;
    
    /**
    * @brief Serializes a specific audio channel to a JSON object.
    * @param output The JSON object to write to.
    * @param audioChannel The `AudioChannel`context.
    * @return True if the operation was successful, false otherwise.
    */
    bool writeChannelToJson(json& output, AudioChannel* audioChannel);

    /**
     * @brief Deserializes the container from a JSON object.
     * @param input The JSON object to read from.
     * @param rebuild Whether to rebuild the container after reading.
     * @return True if the operation was successful, false otherwise.
     */
    bool readFromJson(json& input, bool rebuild) override;

    /**
     * @brief Retrieves the size of the container in units.
     * @return The size of the container in units.
     */
    int getSizeInUnits() override { return static_cast<int>(audioRegions.size() * 2); }

    /**
     * @brief Merges data from a JSON object into the container.
     * @param input The JSON object to merge from.
     */
    void mergeFromJson(json& input, int destinationChannel);

    /**
     * @brief Retrieves all audio regions in the container.
     * @return A constant reference to a vector of shared pointers to `AudioRegion` objects.
     */
    const std::vector<std::shared_ptr<AudioRegion>> &getObjects() const { return audioRegions; }

private:
    /// Reference to the associated audio track.
    AudioTrack &audioTrack;

    /// Reference to the associated audio resource container.
    AudioResourceContainer &audioResourceContainer;

    /// Reference to the associated audio track container.
    AudioTrackContainer &audioTrackContainer;

    /// Shared pointer to the tempo provider.
    std::shared_ptr<TempoProvider> tempoProvider;

    /// Shared pointer to the undo manager.
    std::shared_ptr<juce::UndoManager> undoManager;

    /// The currently selected row number.
    int selectedRowNumber = -1;

    /// Vector of audio regions managed by the container.
    std::vector<std::shared_ptr<AudioRegion>> audioRegions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionContainer)
};

} // namespace audium
