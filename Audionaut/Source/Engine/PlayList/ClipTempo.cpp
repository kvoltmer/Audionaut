//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ClipTempo.h"

#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Analysis/AnalysisWorker.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Resource/AudioResource.h"

namespace audium {

juce::File ClipTempo::sourceFile (const PlayListItem& item)
{
    auto region = item.getRegion();
    if (region == nullptr)
        return {};

    for (const auto& resource : region->getAudioResources())
        if (resource != nullptr)
            return juce::File (resource->getFullPathName());

    return {};
}

float ClipTempo::detectedClipTempo (const PlayListItem& item, const AnalysisProvider& provider)
{
    const auto file = sourceFile (item);
    if (file == juce::File())
        return 0.0f;

    for (auto type : { AnalysisType::Beat, AnalysisType::BeatDegara })
        if (auto bpm = provider.getBpm (type, file); bpm > 0.0f)
            return bpm;

    return 0.0f;
}

int ClipTempo::requestClipTempoDetection (const PlayListItem& item, AnalysisWorker& worker)
{
    const auto file = sourceFile (item);
    if (file == juce::File())
        return 0;

    return worker.enqueue (file, { AnalysisType::BeatDegara });
}

bool ClipTempo::lockToTempo (PlayListItem& item, const AnalysisProvider& provider)
{
    if (item.getClipTempo() <= 0.0)
        if (auto bpm = detectedClipTempo (item, provider); bpm > 0.0f)
            item.setClipTempo (bpm);

    item.setTempoLocked (true);
    return item.getClipTempo() > 0.0;
}

} // namespace audium
