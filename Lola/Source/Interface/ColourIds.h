/*
  ==============================================================================

    ColourIds.h
    Created: 7 Jun 2023 3:46:18pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once


namespace audium {

enum ColourIds
{
    backgroundColourId                = 0x2340000,
    secondaryBackgroundColourId       = 0x2340001,
    defaultTextColourId               = 0x2340002,
    defaultHighlightColourId          = 0x2340008,
    listBoxBackgroundColourId         = 0x2340009,
    gridColourId                      = 0x234000a,
    soloColourId                      = 0x234000b,
    muteColourId                      = 0x234000c,
};

// Waveform Colours:
// iterating 2 palettes where the frist one has less colours to gain more variaty
static int currentWaveFormColour = 0;
static const int numWaveFormColours = 15;
static const juce::uint32 waveFormColours[numWaveFormColours] = {
    0xff70d6ff,0xffff70a6,0xffff9770,0xffffd670,0xffe9ff70, // first palette
    0xfffbf8cc,0xfffde4cf,0xffffcfd2,0xfff1c0e8,0xffcfbaf0,0xffa3c4f3,0xff90dbf4,0xff8eecf5,0xff98f5e1,0xffb9fbc0 // second palette
};

class WaveFormColours {
        
public:
    
    static void resetWaveFormColour();
    static juce::Colour getNewWaveFormColour();
    static juce::Colour getCurrentWaveFormColour();
    static juce::Colour getComplementaryColour(juce::Colour c);
};
} // namespace audium
