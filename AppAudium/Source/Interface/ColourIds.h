/*
  ==============================================================================

    ColourIds.h
    Created: 7 Jun 2023 3:46:18pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

/// TODO: clean up the mess :D

namespace audium {

enum ColourIds
{
    backgroundColourId                = 0x2340000, // used
    secondaryBackgroundColourId       = 0x2340001, // used
    defaultTextColourId               = 0x2340002, // used
    widgetTextColourId                = 0x2340003,
    defaultButtonBackgroundColourId   = 0x2340004,
    secondaryButtonBackgroundColourId = 0x2340005,
    userButtonBackgroundColourId      = 0x2340006,
    defaultIconColourId               = 0x2340007,
    treeIconColourId                  = 0x2340008,
    defaultHighlightColourId          = 0x2340009, // used
    defaultHighlightedTextColourId    = 0x234000a, // used
    codeEditorLineNumberColourId      = 0x234000b,
    activeTabIconColourId             = 0x234000c,
    inactiveTabBackgroundColourId     = 0x234000d,
    inactiveTabIconColourId           = 0x234000e,
    contentHeaderBackgroundColourId   = 0x234000f,
    widgetBackgroundColourId          = 0x2340010,
    secondaryWidgetBackgroundColourId = 0x2340011,
};

// Waveform Colours:
// iterating 2 palettes where the frist one has less colours to gain more variaty
static int currentWaveFormColour = 0;
static const int numWaveFormColours = 15;
static const juce::uint32 waveFormColours[numWaveFormColours] = {
    0xff70d6ff,0xffff70a6,0xffff9770,0xffffd670,0xffe9ff70, // first palette
    0xfffbf8cc,0xfffde4cf,0xffffcfd2,0xfff1c0e8,0xffcfbaf0,0xffa3c4f3,0xff90dbf4,0xff8eecf5,0xff98f5e1,0xffb9fbc0 // second palette
};

static juce::Colour getNewWaveFormColour()
{
    /// simply iteraterate our colour scheme and assign our current waveFormColourSchemecolour
    auto result  = juce::Colour(waveFormColours[currentWaveFormColour++]);
    if (currentWaveFormColour >= numWaveFormColours)
        currentWaveFormColour = 0;
    return result;
}


} // namespace audium
