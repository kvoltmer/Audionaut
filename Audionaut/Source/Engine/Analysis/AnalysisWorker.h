//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>
#include <JuceHeader.h>

#include "Engine/Analysis/AnalysisCache.h"

namespace audium {

class AnalysisProvider;

/**
 * @class AnalysisWorker
 * @brief Runs audio analysis in the background on a low-priority thread.
 *
 * Analysis (BIC segmentation, onset detection, beat tracking) is expensive and
 * must never block the message or audio threads. `AnalysisWorker` owns a single
 * background `juce::TimeSliceThread` and a queue of pending (file, analysis
 * type) jobs. Callers `enqueue()` a file - e.g. `AudioResourceContainer` does so
 * whenever a new audio resource is added - and the worker analyses it later,
 * one job per time slice, delegating to `AnalysisProvider::analyzeFile()`.
 *
 * Results land in the shared `AnalysisCache` (via the provider), so the UI picks
 * them up through the usual `AnalysisProvider::getSegments()` query once ready.
 */
class AnalysisWorker : private juce::TimeSliceClient {

public:
    /**
     * @brief Constructs the worker and starts its background thread.
     * @param analysisProvider The provider used to run each analysis.
     * @param defaultAnalysisTypes The analyses queued when enqueue() is called
     *        without an explicit type list (e.g. for a newly added resource).
     *        Defaults to all four; pass a subset (or an empty list) to change
     *        or disable automatic analysis.
     *
     *        The order matters: jobs run in the order they are queued, so the
     *        analyses the merge is built from (see
     *        AnalysisProvider::getMergeAnalysisTypes) come first. Auto Edit
     *        needs only those, so it becomes available without waiting for the
     *        rest.
     */
    explicit AnalysisWorker(std::shared_ptr<AnalysisProvider> analysisProvider,
                            std::vector<AnalysisType> defaultAnalysisTypes = {
                                AnalysisType::SBic,
                                AnalysisType::BeatDegara,
                                AnalysisType::Onset,
                                AnalysisType::Beat
                            });

    /** @brief Stops the background thread, waiting for any in-flight analysis. */
    ~AnalysisWorker() override;

    /** @brief The analysis types queued for a file when no list is given. */
    const std::vector<AnalysisType>& getDefaultAnalysisTypes() const { return defaultTypes; }

    /**
     * @brief Queues background analysis of @p audioFile for the given types.
     *
     * Duplicate (file, type) jobs already pending are ignored, and the file
     * must exist. Safe to call from the message thread.
     *
     * @param audioFile The audio file to analyse.
     * @param types     The analyses to run.
     */
    void enqueue(const juce::File& audioFile,
                 const std::vector<AnalysisType>& types);

    /**
     * @brief Queues background analysis of @p audioFile using the default
     *        analysis types configured at construction.
     */
    void enqueue(const juce::File& audioFile) { enqueue(audioFile, defaultTypes); }

    /**
     * @brief Cancels all pending analyses of @p audioFile (any type).
     *
     * Call when the file's audio resource is unloaded so the worker doesn't
     * waste time analysing a file nobody references anymore (and which may be
     * about to be deleted). An analysis of the file that is already running is
     * not interrupted; it finishes normally and its result simply lands in the
     * cache. Safe to call from the message thread.
     */
    void cancel(const juce::File& audioFile);

    /**
     * @brief Cancels every pending analysis (e.g. when the project closes).
     *
     * As with cancel(), an analysis already running is not interrupted.
     */
    void cancelAll();

    /** @brief Number of queued analyses not yet started (excludes any running). */
    int getPendingCount() const;

    /** @brief True while an analysis is actively running on the worker thread. */
    bool isBusy() const { return busy.load(); }

    /** @brief Total analyses left to do: queued plus any currently running. */
    int getRemainingCount() const;

    /** @brief Name of the file being analysed right now, or empty if idle. */
    juce::String getCurrentFileName() const;

    /** @brief Type of the analysis running right now, or std::nullopt if idle. */
    std::optional<AnalysisType> getCurrentAnalysisType() const;

private:
    int useTimeSlice() override;

    struct Job {
        juce::File file;
        AnalysisType type;

        bool operator== (const Job& other) const
        {
            return type == other.type && file == other.file;
        }
    };

    std::shared_ptr<AnalysisProvider> analysisProvider;
    std::vector<AnalysisType> defaultTypes;

    mutable std::mutex mutex;
    std::deque<Job> jobs;

    // True only while useTimeSlice() is running an analysis; read by the UI.
    std::atomic<bool> busy { false };

    // Name of the file currently being analysed (empty when idle); guarded by mutex.
    juce::String currentFileName;

    // Type of the analysis currently running (nullopt when idle); guarded by mutex.
    std::optional<AnalysisType> currentAnalysisType;

    juce::TimeSliceThread thread { "analysis worker" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalysisWorker)
};

} // namespace audium
