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
     *
     * The default analysis types are the ones Auto Edit's merge is built from
     * (AnalysisProvider::getMergeAnalysisTypes); the display-only analyses are
     * off unless enabled via setDefaultAnalysisTypes().
     *
     * @param analysisProvider The provider used to run each analysis.
     */
    explicit AnalysisWorker(std::shared_ptr<AnalysisProvider> analysisProvider);

    /**
     * @brief Constructs the worker with an explicit default type list.
     * @param analysisProvider The provider used to run each analysis.
     * @param defaultAnalysisTypes The analyses queued when enqueue() is called
     *        without an explicit type list (e.g. for a newly added resource).
     *
     *        The order matters: jobs run in the order they are queued, so the
     *        analyses the merge is built from (see
     *        AnalysisProvider::getMergeAnalysisTypes) come first. Auto Edit
     *        needs only those, so it becomes available without waiting for the
     *        rest.
     */
    AnalysisWorker(std::shared_ptr<AnalysisProvider> analysisProvider,
                   std::vector<AnalysisType> defaultAnalysisTypes);

    /** @brief Stops the background thread, waiting for any in-flight analysis. */
    ~AnalysisWorker() override;

    /**
     * @brief Every analysis type, in the order jobs should be queued: the
     *        merge analyses first (see the constructor note on ordering).
     */
    static const std::vector<AnalysisType>& canonicalAnalysisTypes();

    /** @brief The analysis types queued for a file when no list is given. */
    std::vector<AnalysisType> getDefaultAnalysisTypes() const;

    /**
     * @brief Replaces the analysis types used by the no-list enqueue().
     *
     * The given set is reordered to canonicalAnalysisTypes() order, so the
     * merge analyses always run first regardless of the caller's ordering.
     * Safe to call from the message thread.
     */
    void setDefaultAnalysisTypes(const std::vector<AnalysisType>& types);

    /**
     * @brief Enables or disables automatic analysis.
     *
     * While disabled, the no-list enqueue() overload is a no-op; explicitly
     * requested analyses (enqueue with a type list) still run.
     */
    void setAutoAnalysisEnabled(bool enabled) { autoAnalysisEnabled = enabled; }

    /** @brief True while the no-list enqueue() overload queues analyses. */
    bool isAutoAnalysisEnabled() const { return autoAnalysisEnabled.load(); }

    /**
     * @brief Queues background analysis of @p audioFile for the given types.
     *
     * Duplicate (file, type) jobs already pending are ignored, and the file
     * must exist. Safe to call from the message thread.
     *
     * @param audioFile The audio file to analyse.
     * @param types     The analyses to run.
     * @returns The number of newly queued jobs.
     */
    int enqueue(const juce::File& audioFile,
                const std::vector<AnalysisType>& types);

    /**
     * @brief Queues background analysis of @p audioFile using the configured
     *        default analysis types.
     *
     * Does nothing while automatic analysis is disabled (see
     * setAutoAnalysisEnabled).
     *
     * @returns The number of newly queued jobs.
     */
    int enqueue(const juce::File& audioFile);

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

    // The types the no-list enqueue() queues; guarded by mutex.
    std::vector<AnalysisType> defaultTypes;

    std::atomic<bool> autoAnalysisEnabled { true };

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
