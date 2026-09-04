//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

namespace audium {

class ClipTransportSource;
class PlayListItem;
class DspClip;
class TempoProvider;

//==============================================================================
/**
 * @struct ClipFadeSpec
 * @brief The complete fade geometry of one clip, in seconds, file/source time
 *        domain. One spec drives live scheduling, project bounce and item
 *        bounce identically.
 *
 * The signed offsets follow ClipDynamics: positive = ramp boundary inside
 * the clip (silent head/tail), negative = outside (the fade extends the
 * audible material past the region window).
 */
struct ClipFadeSpec
{
    double regionStart = 0.0, regionEnd = 0.0;   // the region window
    double fadeIn = 0.0, fadeOut = 0.0;          // ramp end from start / ramp start from end
    double fadeInStart = 0.0, fadeOutEnd = 0.0;  // signed ramp offsets
    double fadeInCurve = 0.5, fadeOutCurve = 0.5; // curve exponents (0.5 = equal power)

    /// The clip's playback speed. Every field above is in source-file
    /// seconds; one source second lasts 1/speedRatio timeline seconds.
    double speedRatio = 1.0;

    double headExtension()  const noexcept { return juce::jmax(0.0, -fadeInStart); }
    double tailExtension()  const noexcept { return juce::jmax(0.0, -fadeOutEnd); }

    /** Portion of the head extension before the source file's first sample -
        the voice cannot render it; it is timeline silence. */
    double preFileSilence() const noexcept { return juce::jmax(0.0, headExtension() - regionStart); }

    /** First file position the voice reads from; never negative. */
    double voiceFileStart() const noexcept { return regionStart - headExtension() + preFileSilence(); }

    /** File position the voice reads to (past EOF renders silent). */
    double voiceFileEnd()   const noexcept { return regionEnd + tailExtension(); }

    /** Full audible length including both extensions. */
    double audibleLength()  const noexcept { return (regionEnd - regionStart) + headExtension() + tailExtension(); }

    /** The item's fade geometry, in seconds (message-thread paths: bounce). */
    static ClipFadeSpec fromPlayListItem (const PlayListItem& item);

    /** The clip's fade geometry converted from clocks to seconds via the
        tempo provider (audio-thread path: live scheduling). Real-time safe. */
    static ClipFadeSpec fromDspClip (const DspClip& dspClip,
                                     const TempoProvider& tempoProvider);
};

/**
 * Configures both clip fades on `source` for a voice scheduled to read from
 * filePositionSeconds (>= spec.voiceFileStart()). The one shared fade
 * configuration used by PlayListScheduler::scheduleClip and
 * VoiceSource::configureDynamics - kept in lockstep by
 * construction. Real-time safe.
 */
void configureClipFades (ClipTransportSource& source,
                         const ClipFadeSpec& spec,
                         double filePositionSeconds,
                         bool reset);

} // namespace audium
