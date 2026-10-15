//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "WaveFormColours.h"

namespace audium {

// Waveform Colours:
// iterating 2 palettes where the frist one has less colours to gain more variaty
static int currentWaveFormColour = 0;
static const int numWaveFormColours = 15;
static const juce::uint32 waveFormColours[numWaveFormColours] = {
    0xff70d6ff,0xffff70a6,0xffff9770,0xffffd670,0xffe9ff70, // first palette
    0xfffbf8cc,0xfffde4cf,0xffffcfd2,0xfff1c0e8,0xffcfbaf0,0xffa3c4f3,0xff90dbf4,0xff8eecf5,0xff98f5e1,0xffb9fbc0 // second palette
};

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
