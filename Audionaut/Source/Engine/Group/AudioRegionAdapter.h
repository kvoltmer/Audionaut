//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <JuceHeader.h>

#include "Engine/TimeContext.h"
#include "Engine/Region/AudioRegionData.h"

namespace audium {

class AudioTrackContainer;
class AudioRegion;

/**
 * @class AudioRegionAdapter
 * @brief Facilitates the management and manipulation of audio regions within an audio track container.
 *
 * The `AudioRegionAdapter` class provides functionality for retrieving, selecting, and modifying
 * audio regions. It supports operations such as deselecting all regions, splitting regions, and
 * creating new regions from a selection. It also allows for range-based selection and provides
 * access to selected rows.
 */
class AudioRegionAdapter
{
public:
    /**
     * @brief Constructs an `AudioRegionAdapter` instance.
     * @param owner Reference to the owning `AudioTrackContainer`.
     */
    AudioRegionAdapter(AudioTrackContainer &owner);

    /**
     * @brief Default destructor for `AudioRegionAdapter`.
     */
    ~AudioRegionAdapter() = default;

    /**
     * @brief Gets all audio regions managed by the adapter.
     * @return A vector of shared pointers to `AudioRegion` instances.
     */
    const std::vector<std::shared_ptr<AudioRegion>> getAudioRegions() const;

    /**
     * @brief Gets all selected audio regions.
     * @return A vector of shared pointers to selected `AudioRegion` instances.
     */
    const std::vector<std::shared_ptr<AudioRegion>> getSelectedAudioRegions() const;

    /**
     * @brief Gets a specific audio region by its row number.
     * @param rowNumber The row number of the audio region.
     * @return A shared pointer to the `AudioRegion` at the specified row.
     */
    std::shared_ptr<AudioRegion> getRegion(int rowNumber) const;

    /**
     * @brief Deselects all audio regions.
     */
    void deselectAll();

    /**
     * @brief Gets the selected rows in the adapter.
     * @return A `juce::SparseSet` of selected row indices.
     */
    juce::SparseSet<int> getSelectedRows() const;

    /**
     * @brief Sets the selected rows in the adapter.
     * @param selectedRows The `juce::SparseSet` of row indices to select.
     */
    void setSelectedRows(juce::SparseSet<int>& selectedRows);

    /**
     * @brief Creates new audio regions from the current selection.
     * @param name The name of the new regions.
     * @param arrangementMode Whether to use arrangement mode.
     */
    void createRegionsFromSelection(juce::String name, bool arrangementMode);

    /**
     * @brief Splits existing regions into separate regions.
     */
    void splitRegions(double pos, audium::TimeContextType context);

    /**
     * @brief Sets the selected range in the adapter.
     * @param pos The range to select.
     * @param context The time context type.
     */
    void setSelectedRange(juce::Range<double> pos, audium::TimeContextType context);

    /**
     * @brief Gets the currently selected range.
     * @param context The time context type.
     * @return The selected range as a `juce::Range<double>`.
     */
    juce::Range<double> getSelectedRange(audium::TimeContextType context) const;


    bool canCreateRegion() const;
    
    /**
     * @brief Checks if any range is currently selected.
     * @return True if a range is selected, false otherwise.
     */
    bool anyRangeSelected() const;
    
    bool canSplitAnyRegionAtPosition(double pos, audium::TimeContextType context) const;

private:
    AudioTrackContainer &owner; ///< Reference to the owning `AudioTrackContainer`.

    AudioRegionData::tRange selectedPositionClocks; ///< The currently selected range in transport clocks.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionAdapter)
};

} // namespace audium
