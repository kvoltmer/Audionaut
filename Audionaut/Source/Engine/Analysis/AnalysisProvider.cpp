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
    auto resources = audioTrack.getAudioResources();
    if (resources.empty())
    {
        std::cout << "AnalysisProvider::analyzeSBic() - track has no audio resources to analyse." << std::endl;
        return;
    }

    for (const auto& resource : resources)
    {
        if (resource == nullptr)
            continue;

        const auto audioFile = juce::File(resource->getFullPathName());

        // Reuse a cached result when the file is unchanged; otherwise run the
        // (expensive) BIC segmentation and cache it for next time.
        std::vector<float> segments;
        if (auto cached = analysisCache->get(audioFile, AnalysisType::SBic))
        {
            segments = std::move(*cached);
            std::cout << "AnalysisProvider::analyzeSBic() - cache hit for "
                      << audioFile.getFileName() << std::endl;
        }
        else
        {
            // The BIC segmentation itself is delegated to the injected SBicSegmenter.
            segments = sBicSegmenter->analyze(audioFile);

            // Only cache successful analyses so failures are retried next time.
            if (! segments.empty())
                analysisCache->put(audioFile, AnalysisType::SBic, segments);
        }

        std::cout << "AnalysisProvider::analyzeSBic() - " << segments.size()
                  << " segment boundary/boundaries for " << audioFile.getFileName() << std::endl;
        for (auto seconds : segments)
            std::cout << "  segment at " << seconds << " s" << std::endl;

        // Keep the result available for later querying (e.g. by the UI).
        analysisResults[AnalysisType::SBic][audioFile.getFullPathName().toStdString()] = std::move(segments);
    }
}

std::vector<float> AnalysisProvider::getSegments(AnalysisType analysisType,
                                                 const juce::File& audioFile) const
{
    const auto typeIt = analysisResults.find(analysisType);
    if (typeIt == analysisResults.end())
        return {};

    const auto fileIt = typeIt->second.find(audioFile.getFullPathName().toStdString());
    if (fileIt == typeIt->second.end())
        return {};

    return fileIt->second;
}

std::unordered_map<std::string, std::vector<float>>
    AnalysisProvider::getSegments(AnalysisType analysisType) const
{
    const auto typeIt = analysisResults.find(analysisType);
    if (typeIt == analysisResults.end())
        return {};

    return typeIt->second;
}

} // namespace audium
