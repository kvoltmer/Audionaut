//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <array>
#include <memory>
#include <vector>

#include "EffectSlot.h"
#include "Engine/Playback/PlaybackDefines.h"

namespace audium
{

/**
 * What the renderer runs on top of the mixer's fixed topology: the insert
 * slots of every bus channel, every track and the master, as plain lists
 * of pointers. Built on the message thread from the model, immutable once
 * published, handed to the audio thread through the MixPlanContainer. The
 * plan keeps every slot it points at alive, so a model rebuild that drops
 * channels cannot free an effect the audio thread is inside.
 *
 * The plan is structure only: what runs where, in which order. Gain, pan,
 * mute and solo keep their own path through the commander, and an effect's
 * parameters are atomics inside the effect, so none of them need a plan.
 * The sends, aux buses and latency delays of the later phases go in here.
 *
 * The spec the slots were prepared for travels with the plan; the renderer
 * skips a plan prepared for another sample rate or a shorter block.
 */
struct MixPlan
{
    struct Channel
    {
        std::vector<EffectSlot*> inserts;
    };

    struct Track
    {
        int firstChannel = 0;
        int numChannels = 0;
        std::vector<EffectSlot*> inserts;
    };

    std::vector<Channel> channels;                       ///< by bus channel
    std::vector<Track> tracks;                           ///< in track order
    std::array<int, MAX_AUDIO_CHANNELS> trackAtChannel;  ///< a track's first bus channel -> its index in tracks, -1 elsewhere
    std::vector<EffectSlot*> master;

    double sampleRate = 0.0;
    int maxBlockSize = 0;

    std::vector<std::shared_ptr<EffectSlot>> keepAlive;   ///< every slot referenced above

    MixPlan() { trackAtChannel.fill (-1); }

    /// Whether any chain has a slot: a plan without is skipped as a whole.
    bool hasInserts() const noexcept
    {
        return ! keepAlive.empty();
    }
};

} // namespace audium
