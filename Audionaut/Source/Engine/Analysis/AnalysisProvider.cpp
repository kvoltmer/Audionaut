/*
  ==============================================================================

    AnalysisProvider.cpp
    Created: 28 Jun 2026 3:53:22pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AnalysisProvider.h"
#include "Engine/Resource/AudioResource.h"


namespace audium {

void AnalysisProvider::analyzeSBic(AudioTrack& audioTrack)
{
    analyzeTrack(audioTrack, AnalysisType::SBic);
}

void AnalysisProvider::analyzeOnsets(AudioTrack& audioTrack)
{
    analyzeTrack(audioTrack, AnalysisType::Onset);
}

void AnalysisProvider::analyzeBeats(AudioTrack& audioTrack)
{
    analyzeTrack(audioTrack, AnalysisType::Beat);
}

void AnalysisProvider::analyzeBeatsDegara(AudioTrack& audioTrack)
{
    analyzeTrack(audioTrack, AnalysisType::BeatDegara);
}

void AnalysisProvider::analyzeTrack(AudioTrack& audioTrack, AnalysisType analysisType)
{
    auto resources = audioTrack.getAudioResources();
    if (resources.empty())
    {
        std::cout << "AnalysisProvider::analyzeTrack() - track has no audio resources to analyse." << std::endl;
        return;
    }

    for (const auto& resource : resources)
    {
        if (resource == nullptr)
            continue;

        analyzeFile(juce::File(resource->getFullPathName()), analysisType);
    }
}

AnalysisProvider::SegmentResult AnalysisProvider::runSegmenter(AnalysisType analysisType,
                                                                const juce::File& audioFile)
{
    switch (analysisType)
    {
        case AnalysisType::SBic:  return { sBicSegmenter->analyze(audioFile), 0.0f };
        case AnalysisType::Onset: return { onsetSegmenter->analyze(audioFile), 0.0f };
        case AnalysisType::Beat:
        {
            auto result = beatSegmenter->analyze(audioFile);
            return { std::move(result.beats), result.bpm };
        }
        case AnalysisType::BeatDegara:
        {
            BeatSegmenter::Parameters params;
            params.method = BeatSegmenter::Method::Degara;
            auto result = beatSegmenter->analyze(audioFile, params);
            return { std::move(result.beats), result.bpm };
        }
    }
    return {};
}

std::vector<float> AnalysisProvider::analyzeFile(const juce::File& audioFile,
                                                 AnalysisType analysisType)
{
    // Reuse a cached result when the file is unchanged; otherwise run the
    // (expensive) analysis and cache it for next time.
    if (auto cached = analysisCache->get(audioFile, analysisType))
    {
        std::cout << "AnalysisProvider::analyzeFile() - cache hit for "
                  << audioFile.getFileName() << std::endl;
        return std::move(*cached);
    }

    SegmentResult result;
    {
        // Serialise the actual segmenter run; a concurrent caller may have
        // computed the same result while we waited, so re-check the cache.
        std::lock_guard<std::mutex> lock(segmenterMutex);
        if (auto cached = analysisCache->get(audioFile, analysisType))
            return std::move(*cached);

        result = runSegmenter(analysisType, audioFile);
    }

    // Only cache successful analyses so failures are retried next time.
    if (! result.segments.empty())
    {
        analysisCache->put(audioFile, analysisType, result.segments, result.bpm);

        // Notify listeners (e.g. AudioClipView) that new results are ready to
        // display. Safe to call from any thread - AnalysisWorker runs this on
        // a background thread and ChangeBroadcaster dispatches asynchronously
        // on the message thread.
        sendChangeMessage();
    }

    std::cout << "AnalysisProvider::analyzeFile() - " << result.segments.size()
              << " boundary/boundaries for " << audioFile.getFileName() << std::endl;

#if 0
    for (auto seconds : result.segments)
        std::cout << "  at " << seconds << " s" << std::endl;
#endif

    // Results are held (and persisted) by the cache - the UI queries them
    // back via getSegments()/getBpm(), so nothing further to store here.
    return result.segments;
}

const std::vector<AnalysisType>& AnalysisProvider::getMergeAnalysisTypes()
{
    // Stream order matters: EventMerger expects the segmentation stream first.
    static const std::vector<AnalysisType> types {
        AnalysisType::SBic,
        AnalysisType::BeatDegara
    };

    return types;
}

std::vector<EventMerger::EventStream>
    AnalysisProvider::makeMergeStreams(const std::vector<float>& sBicSegments,
                                       const std::vector<float>& degaraBeats)
{
    std::vector<EventMerger::EventStream> streams;

    const auto add = [&streams] (const char* label,
                                 EventMerger::Kind kind,
                                 const std::vector<float>& times)
    {
        // An analysis that found nothing contributes no stream, so the labels
        // stay meaningful and the column order stays predictable.
        if (times.empty())
            return;

        EventMerger::EventStream stream;
        stream.label = label;
        stream.kind = kind;
        stream.times = times;

        streams.push_back(std::move(stream));
    };

    // The structural boundaries are the rare events, so they carry the widest
    // kernel and dominate the merge. Beats are the dense event stream they are
    // aligned to.
    add("sbic", EventMerger::Kind::Segmentation, sBicSegments);
    add("beats_degara", EventMerger::Kind::Beat, degaraBeats);

    return streams;
}

std::vector<AnalysisType>
    AnalysisProvider::findMissingMergeAnalyses(const juce::File& audioFile) const
{
    std::vector<AnalysisType> missing;

    for (auto analysisType : getMergeAnalysisTypes())
        if (! analysisCache->get(audioFile, analysisType).has_value())
            missing.push_back(analysisType);

    return missing;
}

EventMerger::Result AnalysisProvider::mergeCachedAnalyses(const juce::File& audioFile,
                                                          float durationSeconds,
                                                          const EventMerger::Parameters& params) const
{
    // Cache-only by design: running three Essentia analyses here would block
    // whichever thread called us, and AnalysisWorker has normally cached them
    // already. A caller wanting to explain the gap asks
    // findMissingMergeAnalyses().
    auto sBicSegments = analysisCache->get(audioFile, AnalysisType::SBic);
    auto degaraBeats  = analysisCache->get(audioFile, AnalysisType::BeatDegara);

    if (! sBicSegments || ! degaraBeats)
        return {};

    auto streams = makeMergeStreams(*sBicSegments, *degaraBeats);

    if (streams.empty())
        return {};

    EventMerger merger;
    return merger.merge(streams, durationSeconds, params);
}

EventMerger::Result AnalysisProvider::mergeCachedAnalyses(const juce::File& audioFile,
                                                          float durationSeconds) const
{
    return mergeCachedAnalyses(audioFile, durationSeconds, EventMerger::Parameters());
}

void AnalysisProvider::setMergePreview(const std::string& audioFilePath,
                                       int trackId,
                                       int playlistItemId,
                                       std::vector<float> boundaries,
                                       double measures)
{
    mergePreviewFilePath = audioFilePath;
    mergePreviewTrackId = trackId;
    mergePreviewPlaylistItemId = playlistItemId;
    mergePreviewBoundaries = std::move(boundaries);
    mergePreviewMeasures = measures;
    sendChangeMessage();
}

void AnalysisProvider::clearMergePreview()
{
    if (mergePreviewFilePath.empty() && mergePreviewBoundaries.empty())
        return;

    mergePreviewFilePath.clear();
    mergePreviewTrackId = -1;
    mergePreviewPlaylistItemId = -1;
    mergePreviewBoundaries.clear();
    mergePreviewMeasures = 0.0;
    sendChangeMessage();
}

std::vector<float> AnalysisProvider::getMergePreview(const juce::File& audioFile,
                                                     int trackId,
                                                     int playlistItemId) const
{
    if (mergePreviewFilePath != audioFile.getFullPathName().toStdString())
        return {};

    // A preview without a clip applies to the whole file and shows on all of
    // its clips; one with a clip shows on that clip alone.
    if (mergePreviewPlaylistItemId >= 0
        && (mergePreviewTrackId != trackId || mergePreviewPlaylistItemId != playlistItemId))
        return {};

    return mergePreviewBoundaries;
}

std::vector<float> AnalysisProvider::getSegments(AnalysisType analysisType,
                                                 const juce::File& audioFile) const
{
    return analysisCache->get(audioFile, analysisType).value_or(std::vector<float>{});
}

std::unordered_map<std::string, std::vector<float>>
    AnalysisProvider::getSegments(AnalysisType analysisType) const
{
    return analysisCache->getAll(analysisType);
}

float AnalysisProvider::getBpm(AnalysisType analysisType, const juce::File& audioFile) const
{
    return analysisCache->getBpm(audioFile, analysisType).value_or(0.0f);
}

} // namespace audium
