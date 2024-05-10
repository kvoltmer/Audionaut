/*
  ==============================================================================

    SnapToGridHandler.cpp
    Created: 25 Mar 2024 3:54:41pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

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
