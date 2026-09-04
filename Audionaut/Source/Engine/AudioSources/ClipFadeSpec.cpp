//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ClipFadeSpec.h"
#include "ClipTransportSource.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Core/DspClip.h"
#include "Engine/Provider/TempoProvider.h"

namespace audium
{

ClipFadeSpec ClipFadeSpec::fromPlayListItem (const PlayListItem& item)
{
    ClipFadeSpec spec;
    auto regionSeconds = item.getRegionData(audium::seconds);
    spec.regionStart = regionSeconds.getStart();
    spec.regionEnd   = regionSeconds.getEnd();
    spec.fadeIn      = item.getDynamics().getFadeIn(audium::seconds);
    spec.fadeOut     = item.getDynamics().getFadeOut(audium::seconds);
    spec.fadeInStart = item.getDynamics().getFadeInStart(audium::seconds);
    spec.fadeOutEnd  = item.getDynamics().getFadeOutEnd(audium::seconds);
    spec.fadeInCurve  = item.getDynamics().getFadeInCurve();
    spec.fadeOutCurve = item.getDynamics().getFadeOutCurve();
    spec.speedRatio   = item.getSpeedRatio();
    return spec;
}

ClipFadeSpec ClipFadeSpec::fromDspClip (const DspClip& dspClip,
                                        const TempoProvider& tempoProvider)
{
    ClipFadeSpec spec;
    auto regionSeconds = dspClip.getRegionData(audium::seconds);
    spec.regionStart = regionSeconds.getStart();
    spec.regionEnd   = regionSeconds.getEnd();
    spec.fadeIn      = tempoProvider.clocksToSeconds(dspClip.dspClipData.clipFadeInClocks);
    spec.fadeOut     = tempoProvider.clocksToSeconds(dspClip.dspClipData.clipFadeOutClocks);
    spec.fadeInStart = tempoProvider.clocksToSeconds(dspClip.dspClipData.clipFadeInStartClocks);
    spec.fadeOutEnd  = tempoProvider.clocksToSeconds(dspClip.dspClipData.clipFadeOutEndClocks);
    spec.fadeInCurve  = dspClip.dspClipData.clipFadeInCurve;
    spec.fadeOutCurve = dspClip.dspClipData.clipFadeOutCurve;
    spec.speedRatio   = dspClip.dspClipData.clipSpeedRatio;
    return spec;
}

void configureClipFades (ClipTransportSource& source,
                         const ClipFadeSpec& spec,
                         double filePositionSeconds,
                         bool reset)
{
    if (spec.fadeIn <= 0.0 && spec.fadeInStart == 0.0) {
        source.clearFadeIn();
    }
    else {
        source.setFadeInCurve(spec.fadeInCurve);
        // a pre-file clamp makes filePosition > regionStart + fadeInStart,
        // which arms the ramp mid-way - the ramp progresses through the
        // pre-file silence
        // the processor runs post-resampling at the device rate, so the
        // source-second geometry is scaled to wall-clock time here - the
        // single conversion point for both the live and the bounce path
        source.setFadeInRamp((spec.fadeIn - spec.fadeInStart) / spec.speedRatio,
                             ((spec.regionStart + spec.fadeInStart) - filePositionSeconds) / spec.speedRatio,
                             reset);
    }

    if (spec.fadeOut <= 0.0 && spec.fadeOutEnd == 0.0) {
        source.clearFadeOut();
    }
    else {
        source.setFadeOutCurve(spec.fadeOutCurve);
        source.setFadeOutRamp((spec.fadeOut - spec.fadeOutEnd) / spec.speedRatio,
                              ((spec.regionEnd - spec.fadeOut) - filePositionSeconds) / spec.speedRatio,
                              reset);
    }
}

} // namespace audium
