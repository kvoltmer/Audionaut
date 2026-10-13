//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <string>
#include <JuceHeader.h>

namespace audium {

/// The stems a separation produces, in the order the Demucs 4-source model
/// emits them. That order is also the order the stem tracks are created in.
enum class Stem
{
    Drums = 0,
    Bass = 1,
    Other = 2,
    Vocals = 3
};

constexpr int numStems = 4;

inline juce::String stemDisplayName (Stem stem)
{
    switch (stem)
    {
        case Stem::Drums:  return "Drums";
        case Stem::Bass:   return "Bass";
        case Stem::Other:  return "Other";
        case Stem::Vocals: return "Vocals";
    }
    jassertfalse;
    return {};
}

/// The identifier used in JSON output and on the command line.
inline std::string stemToString (Stem stem)
{
    return stemDisplayName (stem).toLowerCase().toStdString();
}

inline Stem stemFromIndex (int index)
{
    jassert (index >= 0 && index < numStems);
    return static_cast<Stem> (juce::jlimit (0, numStems - 1, index));
}

/**
 * Progress notifications from a separation.
 *
 * @param fraction  0..1 over the whole job.
 * @param message   The current stage, for a status line.
 * @return          false to cancel. The job then stops as soon as it can,
 *                  leaves the project untouched and reports no error.
 */
using SeparationProgress = std::function<bool (double fraction, const juce::String& message)>;

} // namespace audium
