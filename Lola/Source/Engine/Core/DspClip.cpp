//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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

