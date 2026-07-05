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

namespace audium {

/** @brief The kinds of analysis whose results the cache can hold. */
enum class AnalysisType {
    SBic,   ///< BIC segmentation.
    Onset,  ///< Onset detection.
    Beat    ///< Beat tracking.
};

/**
 * @class AnalysisCache
 * @brief In-memory cache for audio-analysis results keyed by the analysed
 *        file's identity.
 *
 * Analysis (e.g. BIC segmentation) is expensive, so results are cached and
 * reused as long as the underlying file is unchanged. The cache key combines
 * the analysis type with the file's path, size and last-modification time, so
 * editing or replacing the file automatically invalidates its stale entry.
 *
 * The cache is safe to query and update from multiple threads.
 */
class AnalysisCache {

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
     * @brief Stores a result, replacing any existing entry for the same key.
     */
    void put(const juce::File& audioFile,
             AnalysisType analysisType,
             std::vector<float> result);

    /** @brief Removes all cached entries. */
    void clear();

    /** @brief Number of cached entries (mainly useful for testing). */
    size_t size() const;

private:
    static std::string makeKey(const juce::File& audioFile,
                               AnalysisType analysisType);

    mutable std::mutex mutex;
    std::unordered_map<std::string, std::vector<float>> entries;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalysisCache)
};

} // namespace audium
