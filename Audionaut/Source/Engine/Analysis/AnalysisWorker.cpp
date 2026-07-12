//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Engine/Analysis/AnalysisWorker.h"
#include "Engine/Analysis/AnalysisProvider.h"

#include <algorithm>

namespace audium {

AnalysisWorker::AnalysisWorker(std::shared_ptr<AnalysisProvider> analysisProvider_,
                               std::vector<AnalysisType> defaultAnalysisTypes) :
    analysisProvider(std::move(analysisProvider_)),
    defaultTypes(std::move(defaultAnalysisTypes))
{
    // Lowest priority: analysis must never compete with the UI or audio path.
    thread.startThread(juce::Thread::Priority::background);
    thread.addTimeSliceClient(this);
}

AnalysisWorker::~AnalysisWorker()
{
    // Detach before stopping so removeTimeSliceClient() waits for any analysis
    // currently in useTimeSlice() to finish, then tear the thread down.
    thread.removeTimeSliceClient(this);
    thread.stopThread(-1);
}

void AnalysisWorker::enqueue(const juce::File& audioFile,
                             const std::vector<AnalysisType>& types)
{
    if (! audioFile.existsAsFile())
        return;

    {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto type : types)
        {
            Job job { audioFile, type };
            if (std::find(jobs.begin(), jobs.end(), job) == jobs.end())
                jobs.push_back(job);
        }
    }

    // Wake the thread so it picks up the new work without waiting out its idle poll.
    thread.notify();
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
    }
    busy = true;
    if (analysisProvider != nullptr)
        analysisProvider->analyzeFile(job.file, job.type);
    busy = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        currentFileName = {};
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

} // namespace audium
