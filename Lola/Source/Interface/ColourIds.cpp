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

#include <JuceHeader.h>

#include "ColourIds.h"

namespace audium {

void WaveFormColours::resetWaveFormColour()
{
    currentWaveFormColour = 0;
}

juce::Colour WaveFormColours::getNewWaveFormColour()
{
    /// simply iteraterate our colour scheme and assign our current waveFormColourSchemecolour
    auto result  = juce::Colour(waveFormColours[currentWaveFormColour++]);
    if (currentWaveFormColour >= numWaveFormColours)
        currentWaveFormColour = 0;
    return result;
}

juce::Colour WaveFormColours::getCurrentWaveFormColour()
{
    return juce::Colour(waveFormColours[currentWaveFormColour]);
}

juce::Colour WaveFormColours::getComplementaryColour(juce::Colour c)
{
    float c_r = c.getFloatRed();
    float c_g = c.getFloatGreen();
    float c_b = c.getFloatBlue();
    float c_a = c.getFloatAlpha();
    return juce::Colour::fromFloatRGBA(1.f - c_r, 1.f - c_g, 1.f - c_b, c_a);
}

} // namespace audium
