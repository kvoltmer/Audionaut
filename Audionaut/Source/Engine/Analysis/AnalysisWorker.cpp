//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Engine/Analysis/AnalysisWorker.h"
#include "Engine/Analysis/AnalysisProvider.h"

#include <algorithm>

namespace audium {

AnalysisWorker::AnalysisWorker(std::shared_ptr<AnalysisProvider> analysisProvider_) :
    AnalysisWorker(std::move(analysisProvider_), AnalysisProvider::getMergeAnalysisTypes())
{
}

AnalysisWorker::AnalysisWorker(std::shared_ptr<AnalysisProvider> analysisProvider_,
                               std::vector<AnalysisType> defaultAnalysisTypes) :
    analysisProvider(std::move(analysisProvider_)),
    defaultTypes(std::move(defaultAnalysisTypes))
{
    // Lowest priority: analysis must never compete with the UI or audio path.
    thread.startThread(juce::Thread::Priority::background);
    thread.addTimeSliceClient(this);
}

const std::vector<AnalysisType>& AnalysisWorker::canonicalAnalysisTypes()
{
    static const std::vector<AnalysisType> types {
        AnalysisType::SBic,
        AnalysisType::BeatDegara,
        AnalysisType::Onset,
        AnalysisType::Beat
    };
    return types;
}

std::vector<AnalysisType> AnalysisWorker::getDefaultAnalysisTypes() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return defaultTypes;
}

void AnalysisWorker::setDefaultAnalysisTypes(const std::vector<AnalysisType>& types)
{
    std::vector<AnalysisType> canonical;
    for (auto type : canonicalAnalysisTypes())
        if (std::find(types.begin(), types.end(), type) != types.end())
            canonical.push_back(type);

    std::lock_guard<std::mutex> lock(mutex);
    defaultTypes = std::move(canonical);
}

AnalysisWorker::~AnalysisWorker()
{
    // Detach before stopping so removeTimeSliceClient() waits for any analysis
    // currently in useTimeSlice() to finish, then tear the thread down.
    thread.removeTimeSliceClient(this);
    thread.stopThread(-1);
}

int AnalysisWorker::enqueue(const juce::File& audioFile,
                            const std::vector<AnalysisType>& types)
{
    if (! audioFile.existsAsFile())
        return 0;

    int queued = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto type : types)
        {
            Job job { audioFile, type };
            if (std::find(jobs.begin(), jobs.end(), job) == jobs.end())
            {
                jobs.push_back(job);
                ++queued;
            }
        }
    }

    // Wake the thread so it picks up the new work without waiting out its idle poll.
    thread.notify();
    return queued;
}

int AnalysisWorker::enqueue(const juce::File& audioFile)
{
    if (! autoAnalysisEnabled.load())
        return 0;

    // Snapshot outside enqueue()'s lock: the mutex is not recursive.
    std::vector<AnalysisType> types;
    {
        std::lock_guard<std::mutex> lock(mutex);
        types = defaultTypes;
    }
    return enqueue(audioFile, types);
}

void AnalysisWorker::cancel(const juce::File& audioFile)
{
    std::lock_guard<std::mutex> lock(mutex);
    jobs.erase(std::remove_if(jobs.begin(), jobs.end(),
                              [&audioFile] (const Job& job) { return job.file == audioFile; }),
               jobs.end());
}

void AnalysisWorker::cancelAll()
{
    std::lock_guard<std::mutex> lock(mutex);
    jobs.clear();
}

int AnalysisWorker::useTimeSlice()
{
    Job job;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (jobs.empty())
            return 500; // idle: poll again in half a second

        job = jobs.front();
        jobs.pop_front();
    }

    {
        std::lock_guard<std::mutex> lock(mutex);
        currentFileName = job.file.getFileName();
        currentAnalysisType = job.type;
    }
    busy = true;
    if (analysisProvider != nullptr)
        analysisProvider->analyzeFile(job.file, job.type);
    busy = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        currentFileName = {};
        currentAnalysisType = std::nullopt;
    }

    // Come straight back if more work is waiting, otherwise idle.
    std::lock_guard<std::mutex> lock(mutex);
    return jobs.empty() ? 500 : 0;
}

int AnalysisWorker::getPendingCount() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return static_cast<int>(jobs.size());
}

int AnalysisWorker::getRemainingCount() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return static_cast<int>(jobs.size()) + (busy ? 1 : 0);
}

juce::String AnalysisWorker::getCurrentFileName() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return currentFileName;
}

std::optional<AnalysisType> AnalysisWorker::getCurrentAnalysisType() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return currentAnalysisType;
}

} // namespace audium
