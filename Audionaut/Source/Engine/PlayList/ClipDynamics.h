//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <vector>
#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace audium {

class PlayListItem;

/**
 * \class ClipDynamics
 * \brief The dynamics of a playlist item: per-channel clip gain and the fades.
 *
 * Owned by a `PlayListItem`. Gain is stored per destination channel (an empty
 * vector means 1.0 everywhere); the fades are stored in clocks and clamped
 * against each other within the owner's region length. Serializes itself into
 * the owner's JSON.
 */
class ClipDynamics
{

public:

    explicit ClipDynamics(PlayListItem& owner);

    // gain (linear range), per destination channel
    void setGain(int channel, double val, bool continous = false);
    double getGain(int channel) const;
    void onDeleteChannel(int channel);

    // gains only - split/clone don't inherit fades
    void copyGainsFrom(const ClipDynamics& other);

    // value range [0, 1]; returns true if the opposite fade was clamped
    bool setFadeIn(double val);
    double getFadeIn() const;

    // value range [0, 1]; returns true if the opposite fade was clamped
    bool setFadeOut(double val);
    double getFadeOut() const;

    double getFadeInClocks() const
    {
        return fadeInClocks;
    }

    double getFadeOutClocks() const
    {
        return fadeOutClocks;
    }

    void writeToJson(json& output) const;
    void readFromJson(json& input);

private:

    double getRegionLengthClocks() const;

    PlayListItem& owner;

    // Gain per destination channel; empty => 1.0 for every channel
    std::vector<double> gains;

    double fadeInClocks = 0.0;
    double fadeOutClocks = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipDynamics)
};

} // namespace audium
