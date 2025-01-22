
#include "DspClip.h"

namespace audium {

juce::Range<double> DspClip::getRegionData(audium::TimeContextType context) const
{
    if (dspClipData.clipData.regionData.isEmpty()) {
        jassertfalse;
        return juce::Range<double>(0.0, 0.0);
    }
    
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

