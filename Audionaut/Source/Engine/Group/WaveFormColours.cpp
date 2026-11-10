//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "WaveFormColours.h"

namespace audium {

// Waveform Colours: the rotation runs through two palettes, the first with fewer, stronger
// colours to gain more variety, then the pastels
static int currentWaveFormColour = 0;
static const std::vector<juce::uint32> strongPalette { 0xff70d6ff, 0xffff70a6, 0xffff9770, 0xffffd670, 0xffe9ff70 };
static const std::vector<juce::uint32> pastelPalette { 0xfffbf8cc, 0xfffde4cf, 0xffffcfd2, 0xfff1c0e8, 0xffcfbaf0,
                                                      0xffa3c4f3, 0xff90dbf4, 0xff8eecf5, 0xff98f5e1, 0xffb9fbc0 };
static const int numWaveFormColours = static_cast<int> (strongPalette.size() + pastelPalette.size());

static juce::Colour waveFormColour (int index)
{
    const auto strong = static_cast<int> (strongPalette.size());
    return juce::Colour (index < strong ? strongPalette[static_cast<size_t> (index)] : pastelPalette[static_cast<size_t> (index - strong)]);
}

void WaveFormColours::resetWaveFormColour()
{
    currentWaveFormColour = 0;
}

juce::Colour WaveFormColours::getNewWaveFormColour()
{
    /// simply iteraterate our colour scheme and assign our current waveFormColourSchemecolour
    auto result  = waveFormColour (currentWaveFormColour++);
    if (currentWaveFormColour >= numWaveFormColours)
        currentWaveFormColour = 0;
    return result;
}

std::array<std::vector<juce::Colour>, 2> WaveFormColours::getPalettes()
{
    std::array<std::vector<juce::Colour>, 2> palettes;
    for (const auto argb : strongPalette) palettes[0].emplace_back (argb);
    for (const auto argb : pastelPalette) palettes[1].emplace_back (argb);
    return palettes;
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
