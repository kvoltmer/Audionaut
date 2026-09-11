//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "DspClip.h"

namespace audium {

juce::Range<double> DspClip::getRegionData(audium::TimeContextType context) const
{    
    if (context == audium::seconds) {
        return dspClipData.clipData.regionData;
    }
    else if (context == audium::clocks) {
        return tempoProvider->secondsToClocks(dspClipData.clipData.regionData);
    }
    jassertfalse;
    return juce::Range<double>(0.0, 0.0);
}


void DspClip::setRegionData(juce::Range<double> newRegionData, audium::TimeContextType context)
{
    jassert(!newRegionData.isEmpty());
    jassert(newRegionData.getStart() <= newRegionData.getEnd());
    if (context == audium::seconds) {
        dspClipData.clipData.regionData = newRegionData;
    }
    else if (context == audium::clocks) {
        dspClipData.clipData.regionData = tempoProvider->clocksToSeconds(newRegionData);
    }
    
    if (dspClipData.clipData.regionData.getStart() < 0.0) {
        dspClipData.clipData.regionData.setStart(0.0);
    }
}

double DspClip::getAbsolutePosition(audium::TimeContextType context) const
{
    if (context == audium::seconds) {
        return tempoProvider->clocksToSeconds(dspClipData.clipData.absolutePositionClocks);
    }
    else if (context == audium::clocks) {
        return dspClipData.clipData.absolutePositionClocks;
    }
    jassertfalse;
    return 0.0;
}

double DspClip::getHeadExtension(audium::TimeContextType context) const
{
    // pre-file portions are clamped away: the voice cannot start before the
    // source file's first sample, and the gate must not open before the
    // voice can start
    auto headExtSeconds = juce::jmax(0.0, -tempoProvider->clocksToSeconds(dspClipData.clipFadeInStartClocks));
    auto regionStart = dspClipData.clipData.regionData.getStart();

    // the extension is source material: clamp in the source domain, then
    // scale to the timeline
    auto effectiveSeconds = juce::jmin(headExtSeconds, regionStart) / getSpeedRatio();

    if (context == audium::seconds)
        return effectiveSeconds;
    else if (context == audium::clocks)
        return tempoProvider->secondsToClocks(effectiveSeconds);

    jassertfalse;
    return 0.0;
}

double DspClip::getTailExtension(audium::TimeContextType context) const
{
    // source material rings out for source / speed timeline time
    auto tailExtClocks = juce::jmax(0.0, -dspClipData.clipFadeOutEndClocks) / getSpeedRatio();

    if (context == audium::clocks)
        return tailExtClocks;
    else if (context == audium::seconds)
        return tempoProvider->clocksToSeconds(tailExtClocks);

    jassertfalse;
    return 0.0;
}

juce::Range<double> DspClip::getAudibleRange(audium::TimeContextType context) const
{
    auto range = getAbsolutePositionRange(context);
    return juce::Range<double>(range.getStart() - getHeadExtension(context),
                               range.getEnd() + getTailExtension(context));
}

void DspClip::setAbsolutePosition(double newPosition, audium::TimeContextType context)
{
    if (context == audium::seconds) {
        dspClipData.clipData.absolutePositionClocks = tempoProvider->secondsToClocks(newPosition);
    }
    else if (context == audium::clocks) {
        dspClipData.clipData.absolutePositionClocks = newPosition;
    }
    else {
        jassertfalse;
    }
}

} // namespace audium

