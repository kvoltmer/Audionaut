//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#include "SnapToGridHandler.h"

void SnapToGridHandler::publishRange(juce::Range<double> clocks)
{
    clockRange = clocks;
    sendChangeMessage();
}

void SnapToGridHandler::clearRange()
{
    clockRange = juce::Range<double>();
    sendChangeMessage();
}
