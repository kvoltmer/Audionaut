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
