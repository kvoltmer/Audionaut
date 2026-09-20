//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

namespace audium {

class PlayListItem;
class AnalysisProvider;
class AnalysisWorker;

/**
 * Seeding a clip's native tempo from the beat analysis - shared by the
 * Stretch overlay and the clip-speed CLI verb.
 */
namespace ClipTempo {

/// The audio file the item's tempo is detected from (its region's first
/// resource); an invalid File when there is none.
juce::File sourceFile (const PlayListItem& item);

/// The BPM the beat tracking found for the item's source (the multi-feature
/// result first, then Degara's); 0 when the source has not been analysed.
float detectedClipTempo (const PlayListItem& item, const AnalysisProvider& provider);

/// Queues the beat analysis for the item's source explicitly (it runs even
/// while automatic analysis is off). Returns the number of newly queued jobs.
int requestClipTempoDetection (const PlayListItem& item, AnalysisWorker& worker);

/**
 * Locks the item to the project tempo, seeding its clip tempo from the
 * analysis when it is still unknown. Returns whether the clip tempo is known
 * afterwards - if not, the caller should request the detection; the item
 * stays locked at ratio 1.0 until a tempo arrives or the user types one.
 */
bool lockToTempo (PlayListItem& item, const AnalysisProvider& provider);

} // namespace ClipTempo

} // namespace audium
