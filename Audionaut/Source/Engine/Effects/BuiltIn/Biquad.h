//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <array>
#include <vector>

namespace audium
{

/**
 * One biquad of a built-in effect, in transposed direct form II, with a
 * state per channel. The coefficients come from juce_dsp's array makers (no
 * allocation), normalised here by a0; the states are sized at prepare().
 * A biquad that is not active is skipped by its effect, and starts from a
 * cleared state when it comes back.
 *
 * `Float` is the precision of the coefficients and the state: float for a
 * band in the audio's path, double where the corner is so low against the
 * sample rate that a float's rounding would move it (the poles of a 20 Hz
 * filter at 192 kHz lie closer to 1 than a float tells apart).
 */
template <typename Float>
struct Biquad
{
    struct State { Float z1 = 0, z2 = 0; };

    bool active = false;
    Float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    std::vector<State> states;   // one per channel, sized by prepare()

    /// The coefficients as juce_dsp's makers give them: b0, b1, b2, a0, a1, a2. Makes the biquad active.
    void set (const std::array<Float, 6>& unnormalised) noexcept
    {
        const auto a0 = unnormalised[3];
        b0 = unnormalised[0] / a0;
        b1 = unnormalised[1] / a0;
        b2 = unnormalised[2] / a0;
        a1 = unnormalised[4] / a0;
        a2 = unnormalised[5] / a0;
        active = true;
    }

    void clearState() noexcept
    {
        for (auto& state : states)
            state = {};
    }

    /// One sample through the biquad, on a channel's state.
    Float process (State& state, Float in) const noexcept
    {
        const auto out = b0 * in + state.z1;
        state.z1 = b1 * in - a1 * out + state.z2;
        state.z2 = b2 * in - a2 * out;
        return out;
    }
};

} // namespace audium
