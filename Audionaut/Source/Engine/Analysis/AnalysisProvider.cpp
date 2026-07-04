/*
  ==============================================================================

    AnalysisProvider.cpp
    Created: 28 Jun 2026 3:53:22pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AnalysisProvider.h"


namespace audium {

void AnalysisProvider::analyzeSBic()
{
    std::cout << "AnalysisProvider::analyzeSBic() called." << std::endl;

    // The BIC segmentation itself is delegated to the injected SBicSegmenter.
    // TODO: resolve the rendered audio file of the track(s) held by
    //       audioTrackContainer and feed it to the segmenter, e.g.:
    //
    //     auto segments = sBicSegmenter->analyze(audioFile);
    //
    // (Enabling this in the app target additionally requires linking Essentia
    //  into the Projucer exporters, which currently only the test target does.)
}

} // namespace audium
