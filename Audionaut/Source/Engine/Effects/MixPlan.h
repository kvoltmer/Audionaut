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
#include "PdcDelay.h"
#include "SendList.h"
#include "Engine/Playback/PlaybackDefines.h"

namespace audium
{

/**
 * What the renderer runs on top of the mixer's fixed topology: the insert
 * slots of every track, every aux bus and the master, as plain lists of
 * pointers. Built on the message thread from the model, immutable once
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
 * new plan.
 *
 * Latency compensation lives in the plan as well: every slot's latency is
 * known once prepared, so the planner works out, for the three points where
 * signals of different latency meet, how long the earlier one has to wait
 * (see Latency), and hands each waiting signal a PdcDelay of that length
 * on the job that owns it. The delays are state, so the planner carries
 * them over from the previous plan where nothing about them changed, and a
 * rebuild does not interrupt the signal in flight.
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
        std::vector<Send> sends;

        /// Waits on the way to a direct out for the main mix's aux and master stages, which the direct out skips; nullptr when none is needed.
        std::shared_ptr<PdcDelay> directOutDelay;
    };

    struct Track
    {
        int id = -1;              ///< the AudioTrack's id, the key the delays are carried over by
        int firstChannel = 0;
        int numChannels = 0;
        std::vector<EffectSlot*> inserts;   ///< over all of the track's bus channels at once

        /// Waits after the inserts for the latest track, on all of its bus channels; nullptr when none is needed.
        std::shared_ptr<PdcDelay> preFaderDelay;
    };

    /**
     * One aux bus: the send pairs that feed it, in track order (so the sum
     * is the same for any number of workers), its inserts over the stereo
     * sum, and the return into the main mix: gain, pan, mute and solo
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

        /// Waits after the inserts for the latest aux chain; nullptr when none is needed.
        std::shared_ptr<PdcDelay> returnDelay;
    };

    /**
     * How late the signal is at the points of the topology where it is
     * summed, in samples, all from the slots' latencies alone:
     *
     *  - preFader: after the track inserts, every track is as late as the
     *    latest one (the fader point, where the sends, the direct outs and
     *    the main mix branch off);
     *  - aux: the aux returns join the dry main mix as late as the latest aux
     *    chain, the dry mix and the quicker returns having waited for it;
     *  - master: what the master inserts add on top.
     *
     * The main mix leaves preFader + aux + master late, a direct out at
     * preFader + aux + master as well (it waits for the stages it skips), the
     * stems of a stem export at preFader. Link sync and recording placement
     * take the figure for the output they go through, a bounce drops as many
     * samples from its start.
     */
    struct Latency
    {
        int preFader = 0;
        int aux = 0;
        int master = 0;

        int atMainOutput() const noexcept { return preFader + aux + master; }
        int atStemOutput() const noexcept { return preFader; }
        int atOutput (bool stemExport) const noexcept { return stemExport ? atStemOutput() : atMainOutput(); }
    };

    std::vector<Channel> channels;                       ///< by bus channel
    std::vector<Track> tracks;                           ///< in track order
    std::array<int, MAX_AUDIO_CHANNELS> trackAtChannel;  ///< a track's first bus channel -> its index in tracks, -1 elsewhere
    std::vector<Aux> auxes;                              ///< in strip order
    std::vector<EffectSlot*> master;

    Latency latency;                                     ///< what the delays below realise
    std::shared_ptr<PdcDelay> mainDryDelay;              ///< the dry main mix waits for the latest aux chain; nullptr when none is needed

    /**
     * The longest ring-out the chains add after their last input, in
     * seconds, at the output asked for: Effect::getTailSeconds summed along
     * a path, the longest path at each summing point - up to the fader
     * point for the stems of a stem export, through the aux and master
     * stages for the main mix (the split of Latency::atOutput). A bypassed
     * slot rings out nothing. Read from the effects as they are now, not as
     * they were when the plan was built - a delay time changed since counts
     * - so a bounce that runs on for the tail takes the current figure.
     */
    double tailSeconds (bool stemExport) const noexcept
    {
        const auto tailOf = [] (const std::vector<EffectSlot*>& slots)
        {
            auto seconds = 0.0;
            for (const auto* slot : slots)
                if (slot->isEnabled())
                    seconds += std::max (0.0, slot->getEffect().getTailSeconds());
            return seconds;
        };

        auto beforeFader = 0.0;
        for (const auto& track : tracks)
            beforeFader = std::max (beforeFader, tailOf (track.inserts));
        if (stemExport)
            return beforeFader;

        auto ofAuxes = 0.0;
        for (const auto& aux : auxes)
            ofAuxes = std::max (ofAuxes, tailOf (aux.inserts));
        return beforeFader + ofAuxes + tailOf (master);
    }

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

    /// Render thread, nothing else rendering: every slot and every delay starts from rest (a bounce about to begin). The structure stays; what resets is render state.
    void resetState() const noexcept
    {
        for (auto& slot : keepAlive)
            slot->reset();
        for (const auto& track : tracks)
            if (track.preFaderDelay != nullptr)
                track.preFaderDelay->reset();
        for (const auto& channel : channels)
            if (channel.directOutDelay != nullptr)
                channel.directOutDelay->reset();
        for (const auto& aux : auxes)
            if (aux.returnDelay != nullptr)
                aux.returnDelay->reset();
        if (mainDryDelay != nullptr)
            mainDryDelay->reset();
    }

    /**
     * Whether the plan changes the mix: any chain has a slot, any channel
     * sends, or an aux return is soloed (which silences the dry mix). A plan
     * without any of these is skipped as a whole.
     */
    bool hasWork() const noexcept
    {
        return ! keepAlive.empty() || numSendPairs > 0 || anyAuxSolo();
    }

    /// A send's gain as its state says: the level, or silence while muted.
    static float sendGainOf (const SendState& state) noexcept
    {
        return state.mute.load (std::memory_order_relaxed) ? 0.0f : state.level.load (std::memory_order_relaxed);
    }

    /**
     * An aux return's gain per side: the return gain spread by the pan
     * (the side the pan turns away from is attenuated, the other stays),
     * silence while the aux is muted or while another aux is soloed and this
     * one is not.
     */
    static void auxGainsOf (const AuxState& state, bool anyAuxSolo, float& left, float& right) noexcept
    {
        const auto gain = auxGainOf (state, anyAuxSolo);
        const auto pan = state.pan.load (std::memory_order_relaxed);
        left = gain * juce::jmin (1.0f, 1.0f - pan);
        right = gain * juce::jmin (1.0f, 1.0f + pan);
    }

    /// The return gain before pan: 0 while the aux is muted or another one is soloed. What a mono main mix applies.
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
