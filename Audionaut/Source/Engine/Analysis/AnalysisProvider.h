//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Analysis/SBicSegmenter.h"

namespace audium {

class AnalysisProvider {


public:
    AnalysisProvider(AudioTrackContainer &audioTrackContainer_,
                     std::shared_ptr<SBicSegmenter> sBicSegmenter_) :
        audioTrackContainer(audioTrackContainer_),
        sBicSegmenter(sBicSegmenter_)
    {}


    void analyzeSBic(AudioTrack& audioTrack);

private:
    AudioTrackContainer &audioTrackContainer;
    std::shared_ptr<SBicSegmenter> sBicSegmenter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalysisProvider)
};

} // namespace audium
