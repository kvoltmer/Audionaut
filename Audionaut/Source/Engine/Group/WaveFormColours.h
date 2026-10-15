//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

namespace audium {

/**
 * @class WaveFormColours
 * @brief The palette new tracks take their colour from.
 *
 * A track's colour is part of the project (AudioTrackViewState), so handing
 * them out is the model's job, not the views': the container asks here when
 * it creates a track, and a new project starts the rotation over. The views
 * only read the colours back.
 */
class WaveFormColours {

public:

    static void resetWaveFormColour();
    static juce::Colour getNewWaveFormColour();
    static juce::Colour getCurrentWaveFormColour();
    static juce::Colour getComplementaryColour(juce::Colour c);
};

} // namespace audium
