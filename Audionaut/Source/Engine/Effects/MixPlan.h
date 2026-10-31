//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "AuxBus.h"
#include "EffectSlot.h"
#include "SendList.h"
#include "Engine/Playback/PlaybackDefines.h"

namespace audium
{

/**
 * What the renderer runs on top of the mixer's fixed topology: the insert
 * slots of every bus channel, every track and the master, as plain lists
 * of pointers. Built on the message thread from the model, immutable once
 * published, adopted by the render thread through a command in the same
 * queue as the channel data, so a block sees the plan together with the
 * layout it was built for (see MixPlanContainer). The plan keeps every
 * slot it points at alive, so a model rebuild that drops channels cannot
 * free an effect the audio thread is inside.
 *
 * The plan is structure only: what runs where, in which order - which
 * includes the sends of every bus channel and the aux buses they feed,
 * with the buffers between them sized here, once, so the render thread
 * allocates nothing. Gain, pan, mute and solo keep their own path through
 * the commander; an effect's parameters, a send's level and an aux's
 * return values are atomics the plan points at, so none of them need a
 * new plan. The latency delays of a later phase go in here too.
 *
 * The spec the slots were prepared for travels with the plan; the renderer
 * skips a plan prepared for another sample rate or a shorter block. Every
 * slot counts the plans that point at it, so the planner knows when a slot
 * can be prepared again in place and when it has to wait.
 */
struct MixPlan
{
    /**
     * One bus channel's send into one aux: the pair of sendOutput it writes
     * (its own, so the track jobs never share a buffer) and the level,
     * ramped by a gain that only the job rendering the channel touches.
     */
    struct Send
    {
        int aux = 0;                    ///< index into auxes
        int pair = 0;                   ///< stereo pair in sendOutput: channels 2 * pair, 2 * pair + 1
        SendState* state = nullptr;
        mutable juce::dsp::Gain<float> gain;
    };

    struct Channel
    {
        std::vector<EffectSlot*> inserts;
        std::vector<Send> sends;
    };

    struct Track
    {
        int firstChannel = 0;
        int numChannels = 0;
        std::vector<EffectSlot*> inserts;
    };

    /**
     * One aux bus: the send pairs that feed it, in track order (so the sum
     * is the same for any number of workers), its inserts over the stereo
     * sum, and the return into the main mix: gain, balance, mute and solo
     * from the state, ramped by one gain per side that only the job
     * rendering the aux touches.
     */
    struct Aux
    {
        int id = -1;
        AuxState* state = nullptr;
        std::vector<int> pairs;
        std::vector<EffectSlot*> inserts;
        mutable juce::dsp::Gain<float> gainLeft, gainRight;
    };

    std::vector<Channel> channels;                       ///< by bus channel
    std::vector<Track> tracks;                           ///< in track order
    std::array<int, MAX_AUDIO_CHANNELS> trackAtChannel;  ///< a track's first bus channel -> its index in tracks, -1 elsewhere
    std::vector<Aux> auxes;                              ///< in strip order
    std::vector<EffectSlot*> master;

    int numSendPairs = 0;
    mutable juce::AudioBuffer<float> sendOutput;          ///< 2 * numSendPairs channels, maxBlockSize: what the sends write, what the auxes sum
    mutable juce::AudioBuffer<float> auxOutput;           ///< 2 * auxes.size() channels, maxBlockSize: the aux returns, summed into the main mix

    double sampleRate = 0.0;
    int maxBlockSize = 0;
    std::uint64_t serial = 0;                             ///< commit order; the container frees a retired plan once a later one is adopted

    std::vector<std::shared_ptr<EffectSlot>> keepAlive;   ///< every slot referenced above
    std::vector<std::shared_ptr<SendState>> keepAliveSends;   ///< every send state referenced above
    std::vector<std::shared_ptr<AuxState>> keepAliveAuxes;    ///< every aux state referenced above

    MixPlan() { trackAtChannel.fill (-1); }

    ~MixPlan()
    {
        for (auto& slot : keepAlive)
            slot->removePlanReference();
    }

    /// Keeps the slot alive for as long as the plan is, and counts the plan on it.
    void retain (std::shared_ptr<EffectSlot> slot)
    {
        slot->addPlanReference();
        keepAlive.push_back (std::move (slot));
    }

    /// Whether any chain has a slot or any channel sends: a plan without either is skipped as a whole.
    bool hasWork() const noexcept
    {
        return ! keepAlive.empty() || numSendPairs > 0;
    }

    /// A send's gain as its state says: the level, or silence while muted.
    static float sendGainOf (const SendState& state) noexcept
    {
        return state.mute.load (std::memory_order_relaxed) ? 0.0f : state.level.load (std::memory_order_relaxed);
    }

    /**
     * An aux return's gain per side: the return gain spread by the balance
     * (the side the balance turns away from is attenuated, the other stays),
     * silence while the aux is muted or while another aux is soloed and this
     * one is not.
     */
    static void auxGainsOf (const AuxState& state, bool anyAuxSolo, float& left, float& right) noexcept
    {
        const auto gain = auxGainOf (state, anyAuxSolo);
        const auto balance = state.balance.load (std::memory_order_relaxed);
        left = gain * juce::jmin (1.0f, 1.0f - balance);
        right = gain * juce::jmin (1.0f, 1.0f + balance);
    }

    /// The return gain before balance: 0 while the aux is muted or another one is soloed. What a mono main mix applies.
    static float auxGainOf (const AuxState& state, bool anyAuxSolo) noexcept
    {
        const auto mute = state.mute.load (std::memory_order_relaxed);
        const auto solo = state.solo.load (std::memory_order_relaxed);
        if (mute || (anyAuxSolo && ! solo))
            return 0.0f;
        return state.gain.load (std::memory_order_relaxed);
    }

    /// Whether any aux return is soloed: then only the soloed returns reach the main mix.
    bool anyAuxSolo() const noexcept
    {
        for (const auto& aux : auxes)
            if (aux.state->solo.load (std::memory_order_relaxed))
                return true;
        return false;
    }

    JUCE_DECLARE_NON_COPYABLE (MixPlan)
};

} // namespace audium
