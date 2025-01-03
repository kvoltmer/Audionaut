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
