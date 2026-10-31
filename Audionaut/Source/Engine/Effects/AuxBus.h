//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <JuceHeader.h>

#include "InsertChain.h"

namespace audium
{

class MixerPlanner;

/**
 * The values of one aux bus the render thread reads per block: return
 * gain (linear), balance (-1 left .. 1 right), mute and solo, and the
 * return meter it writes. Atomics, so a change needs no new plan; the plan
 * holds a shared_ptr to the state, so a model rebuild under a live plan
 * cannot free it.
 */
struct AuxState
{
    std::atomic<float> gain { 1.0f };
    std::atomic<float> balance { 0.0f };
    std::atomic<bool> mute { false };
    std::atomic<bool> solo { false };
    std::atomic<float> level[2] { 0.0f, 0.0f };   ///< the return's peak per block, left and right
};

/**
 * A stereo send destination with its own strip: a stable id the sends
 * refer to, a name, an insert chain and the return stage (gain, balance,
 * mute, solo) into the main mix. Every project has "Aux 1" and "Aux 2"
 * from the start (AuxBusContainer); more can be added, any can go.
 *
 * JSON: {"id", "name", "gain", "balance", "mute", "solo", "inserts": [...]},
 * one object per bus in the container's "aux_buses" array.
 */
class AuxBus
{
public:
    AuxBus (int id_, std::string name_, MixerPlanner* planner) :
        id (id_),
        name (std::move (name_)),
        state (std::make_shared<AuxState>()),
        inserts (planner)
    {
    }

    int getId() const noexcept { return id; }

    const std::string& getName() const noexcept { return name; }
    void setName (std::string newName) { name = std::move (newName); }

    float getGain() const noexcept { return state->gain.load (std::memory_order_relaxed); }
    void setGain (float gain) noexcept { state->gain.store (juce::jmax (0.0f, gain), std::memory_order_relaxed); }
    float getBalance() const noexcept { return state->balance.load (std::memory_order_relaxed); }
    void setBalance (float balance) noexcept { state->balance.store (juce::jlimit (-1.0f, 1.0f, balance), std::memory_order_relaxed); }
    bool getMute() const noexcept { return state->mute.load (std::memory_order_relaxed); }
    void setMute (bool mute) noexcept { state->mute.store (mute, std::memory_order_relaxed); }
    bool getSolo() const noexcept { return state->solo.load (std::memory_order_relaxed); }
    void setSolo (bool solo) noexcept { state->solo.store (solo, std::memory_order_relaxed); }

    /// The return's last peak, 0 = left, 1 = right (written by the render thread).
    float getLevel (int channel) const noexcept { return state->level[channel == 0 ? 0 : 1].load (std::memory_order_relaxed); }

    const std::shared_ptr<AuxState>& getState() const noexcept { return state; }

    InsertChain& getInserts() noexcept { return inserts; }
    const InsertChain& getInserts() const noexcept { return inserts; }

    void setPlanner (MixerPlanner* planner) noexcept { inserts.setPlanner (planner); }

    void writeToJson (json& output) const;
    /// Reads name, the return values and the inserts; the id is the container's business.
    void readFromJson (const json& input);

private:
    const int id;
    std::string name;
    std::shared_ptr<AuxState> state;
    InsertChain inserts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuxBus)
};

} // namespace audium
