//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ClipFadeSpec.h"
#include "ClipTransportSource.h"

namespace audium
{

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
        source.setFadeInRamp(spec.fadeIn - spec.fadeInStart,
                             (spec.regionStart + spec.fadeInStart) - filePositionSeconds,
                             reset);
    }

    if (spec.fadeOut <= 0.0 && spec.fadeOutEnd == 0.0) {
        source.clearFadeOut();
    }
    else {
        source.setFadeOutCurve(spec.fadeOutCurve);
        source.setFadeOutRamp(spec.fadeOut - spec.fadeOutEnd,
                              (spec.regionEnd - spec.fadeOut) - filePositionSeconds,
                              reset);
    }
}

} // namespace audium
