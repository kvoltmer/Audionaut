//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <JuceHeader.h>

#include "Engine/Streamable.h"

namespace audium {

/** @brief The kinds of analysis whose results the cache can hold. */
enum class AnalysisType {
    SBic,       ///< BIC segmentation.
    Onset,      ///< Onset detection.
    Beat,       ///< Beat tracking (BeatSegmenter, Method::MultiFeature).
    BeatDegara  ///< Beat tracking (BeatSegmenter, Method::Degara).
};

/** @brief Stable, human-readable name for an analysis type (used in JSON). */
std::string analysisTypeToString(AnalysisType analysisType);

/** @brief Parses an analysis type name; std::nullopt if unrecognised. */
std::optional<AnalysisType> analysisTypeFromString(const std::string& name);

/**
 * @class AnalysisCache
 * @brief Persistent store for audio-analysis results keyed by the analysed
 *        file's identity.
 *
 * Analysis (e.g. BIC segmentation) is expensive, so results are cached and
 * reused as long as the underlying file is unchanged. The cache key combines
 * the analysis type with the file's path, size and last-modification time, so
 * editing or replacing the file automatically invalidates its stale entry.
 *
 * The cache is `Streamable`: it serialises its entries to/from JSON and can
 * save/load them as an `AnalysisData.json` sidecar in the project folder.
 * File paths are stored *relative to the project folder*, so results remain
 * valid if the whole project package is moved on disk. In memory the entries
 * stay keyed by absolute path.
 *
 * The cache is safe to query and update from multiple threads.
 */
class AnalysisCache : public Streamable {

public:
    AnalysisCache() = default;

    /**
     * @brief Looks up a cached result.
     * @param audioFile     The analysed audio file.
     * @param analysisType  The kind of analysis.
     * @return The cached result, or std::nullopt on a miss.
     */
    std::optional<std::vector<float>> get(const juce::File& audioFile,
                                          AnalysisType analysisType) const;

    /**
     * @brief Looks up a cached BPM estimate (set for beat-tracking analyses).
     * @param audioFile     The analysed audio file.
     * @param analysisType  The kind of analysis.
     * @return The cached BPM estimate, or std::nullopt on a miss.
     */
    std::optional<float> getBpm(const juce::File& audioFile,
                                AnalysisType analysisType) const;

    /**
     * @brief All results for an analysis type, keyed by analysed file path.
     * @return The per-file results, or an empty map if nothing has been
     *         cached with that type.
     */
    std::unordered_map<std::string, std::vector<float>>
        getAll(AnalysisType analysisType) const;

    /**
     * @brief Stores a result, replacing any existing entry for the same key.
     * @param bpm Overall BPM estimate to store alongside the segments (only
     *            meaningful for beat-tracking analyses); defaults to 0.
     */
    void put(const juce::File& audioFile,
             AnalysisType analysisType,
             std::vector<float> result,
             float bpm = 0.0f);

    /**
     * @brief Re-points cached entries at audio files that have been relocated
     *        into a new folder (e.g. by a project Save-As).
     *
     * For each entry, if a file with the same name now exists in
     * @p newAudioFolder, the entry is re-keyed to that file and its size/
     * modification time refreshed, so persisted results survive the move.
     * Entries whose file is not found there are left untouched.
     */
    void rebaseAudioFolder(const juce::File& newAudioFolder);

    /** @brief Removes all cached entries. */
    void clear();

    /** @brief Number of cached entries (mainly useful for testing). */
    size_t size() const;

    /**
     * @brief Writes AnalysisData.json into the given project folder.
     *
     * A no-op returning true when the cache is empty (nothing to persist).
     *
     * @param projectFolder The project package directory.
     * @return True on success (including the empty no-op), false on I/O error.
     */
    bool saveToFolder(const juce::File& projectFolder);

    /**
     * @brief Clears the cache, then loads AnalysisData.json from the given
     *        project folder if it exists.
     *
     * A missing file is a clean no-op (returns true) so projects without a
     * sidecar simply start empty.
     *
     * @param projectFolder The project package directory.
     * @return True on success (including the missing-file no-op), false on error.
     */
    bool loadFromFolder(const juce::File& projectFolder);

    /** @name Streamable */
    ///@{
    bool writeToJson(json& output) override;
    bool readFromJson(json& input, bool rebuild) override;
    int getSizeInUnits() override;
    ///@}

    /** @brief File name of the analysis sidecar within a project folder. */
    static const char* fileName;

private:
    /** @brief A cached result plus the file identity it was computed from. */
    struct Entry {
        AnalysisType type;
        std::string path;
        juce::int64 size;
        juce::int64 modificationTime;
        std::vector<float> segments;
        float bpm = 0.0f;
    };

    static std::string makeKey(const juce::File& audioFile,
                               AnalysisType analysisType);
    static std::string makeKey(const juce::String& path,
                               juce::int64 size,
                               juce::int64 modificationTime,
                               AnalysisType analysisType);

    mutable std::mutex mutex;
    std::unordered_map<std::string, Entry> entries;

    // Base folder used to (de)serialise entry paths as project-relative. Set
    // only for the duration of saveToFolder/loadFromFolder; an invalid File()
    // means paths are read/written as absolute.
    juce::File serializationFolder;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalysisCache)
};

} // namespace audium
