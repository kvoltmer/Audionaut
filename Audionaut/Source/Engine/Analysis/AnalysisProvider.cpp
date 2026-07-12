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

std::vector<float> AnalysisProvider::runSegmenter(AnalysisType analysisType,
                                                  const juce::File& audioFile)
{
    switch (analysisType)
    {
        case AnalysisType::SBic:  return sBicSegmenter->analyze(audioFile);
        case AnalysisType::Onset: return onsetSegmenter->analyze(audioFile);
        case AnalysisType::Beat:  return beatSegmenter->analyze(audioFile);
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

    std::vector<float> segments;
    {
        // Serialise the actual segmenter run; a concurrent caller may have
        // computed the same result while we waited, so re-check the cache.
        std::lock_guard<std::mutex> lock(segmenterMutex);
        if (auto cached = analysisCache->get(audioFile, analysisType))
            return std::move(*cached);

        segments = runSegmenter(analysisType, audioFile);
    }

    // Only cache successful analyses so failures are retried next time.
    if (! segments.empty())
        analysisCache->put(audioFile, analysisType, segments);

    std::cout << "AnalysisProvider::analyzeFile() - " << segments.size()
              << " boundary/boundaries for " << audioFile.getFileName() << std::endl;
    
#if 0
    for (auto seconds : segments)
        std::cout << "  at " << seconds << " s" << std::endl;
#endif
    
    // Results are held (and persisted) by the cache - the UI queries them
    // back via getSegments(), so nothing further to store here.
    return segments;
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

} // namespace audium
