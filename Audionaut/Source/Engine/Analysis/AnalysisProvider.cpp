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
    // The BIC segmentation itself is delegated to the injected SBicSegmenter.
    analyzeResources(audioTrack, AnalysisType::SBic,
                     [this](const juce::File& file) { return sBicSegmenter->analyze(file); });
}

void AnalysisProvider::analyzeOnsets(AudioTrack& audioTrack)
{
    // Onset detection is delegated to the injected OnsetSegmenter.
    analyzeResources(audioTrack, AnalysisType::Onset,
                     [this](const juce::File& file) { return onsetSegmenter->analyze(file); });
}

void AnalysisProvider::analyzeBeats(AudioTrack& audioTrack)
{
    // Beat tracking is delegated to the injected BeatSegmenter.
    analyzeResources(audioTrack, AnalysisType::Beat,
                     [this](const juce::File& file) { return beatSegmenter->analyze(file); });
}

void AnalysisProvider::analyzeResources(AudioTrack& audioTrack,
                                        AnalysisType analysisType,
                                        const std::function<std::vector<float>(const juce::File&)>& segmenter)
{
    auto resources = audioTrack.getAudioResources();
    if (resources.empty())
    {
        std::cout << "AnalysisProvider::analyzeResources() - track has no audio resources to analyse." << std::endl;
        return;
    }

    for (const auto& resource : resources)
    {
        if (resource == nullptr)
            continue;

        const auto audioFile = juce::File(resource->getFullPathName());

        // Reuse a cached result when the file is unchanged; otherwise run the
        // (expensive) analysis and cache it for next time.
        std::vector<float> segments;
        if (auto cached = analysisCache->get(audioFile, analysisType))
        {
            segments = std::move(*cached);
            std::cout << "AnalysisProvider::analyzeResources() - cache hit for "
                      << audioFile.getFileName() << std::endl;
        }
        else
        {
            segments = segmenter(audioFile);

            // Only cache successful analyses so failures are retried next time.
            if (! segments.empty())
                analysisCache->put(audioFile, analysisType, segments);
        }

        std::cout << "AnalysisProvider::analyzeResources() - " << segments.size()
                  << " boundary/boundaries for " << audioFile.getFileName() << std::endl;
        for (auto seconds : segments)
            std::cout << "  at " << seconds << " s" << std::endl;

        // Results are held (and persisted) by the cache - the UI queries them
        // back via getSegments(), so nothing further to store here.
    }
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
