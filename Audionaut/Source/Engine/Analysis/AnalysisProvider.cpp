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

    const auto audioFile = juce::File(resources.front()->getFullPathName());

    // The BIC segmentation itself is delegated to the injected SBicSegmenter.
    const auto segments = sBicSegmenter->analyze(audioFile);

    std::cout << "AnalysisProvider::analyzeSBic() - " << segments.size()
              << " segment boundary/boundaries for " << audioFile.getFileName() << std::endl;
    for (auto seconds : segments)
        std::cout << "  segment at " << seconds << " s" << std::endl;
}

} // namespace audium
