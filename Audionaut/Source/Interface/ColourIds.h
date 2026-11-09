//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Engine/Group/WaveFormColours.h"

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
    panIndicatorColourId              = 0x234000d,   ///< the pan dial's value arc and centre mark
    dividerColourId                   = 0x234000e,   ///< the bands that frame the window: the header, the mixer's header bar, the right panel's footer
};

/// The accent colour of a toggled-on button: the header's Link, Play and
/// Loop buttons and the clip overlays' toggles.
inline constexpr juce::uint32 toggledOnColour = 0xff12a4e2;

} // namespace audium
