//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "EffectSlot.h"

namespace audium
{

class MixerPlanner;

/**
 * The ordered inserts of one bus channel, track, aux bus or the master: a
 * list of effect slots the mix plan is built from. Every change of the
 * structure (add, remove, move) asks the planner for a new plan; enabling
 * and parameter changes are atomics the running plan picks up.
 *
 * The JSON form is an array of {"type", "enabled", "state"} under the key
 * "inserts", written only when there is something in it, so a project
 * without effects serializes as before. readFromJson keeps the effect
 * instances where the types line up, so an undo step or a hosted edit that
 * changes a value does not rebuild the chain.
 */
class InsertChain
{
public:
    explicit InsertChain (MixerPlanner* planner_ = nullptr) : planner (planner_) {}

    /// The planner that creates effects and rebuilds the plan; nullptr makes the chain inert (tests without one).
    void setPlanner (MixerPlanner* planner_) noexcept { planner = planner_; }

    int size() const noexcept { return static_cast<int> (slots.size()); }
    bool isEmpty() const noexcept { return slots.empty(); }
    const std::vector<std::shared_ptr<EffectSlot>>& getSlots() const noexcept { return slots; }

    /// The slot at the index, nullptr out of range.
    std::shared_ptr<EffectSlot> getSlot (int index) const noexcept;

    /// Adds an effect of the type at the index (-1 = the end); nullptr for an unknown type or without a planner.
    std::shared_ptr<EffectSlot> add (const std::string& typeId, int at = -1);

    bool remove (int index);
    bool move (int from, int to);
    bool setEnabled (int index, bool enabled);
    void clear();

    /// Writes the chain under `key` when it has slots; a project without effects serializes as before.
    void writeToJson (json& output, const char* key = "inserts") const;
    /// Reads the chain under `key` (an absent key empties it), keeping instances where the types line up.
    void readFromJson (const json& input, const char* key = "inserts");

    /// Planner only, while building a plan: a fresh instance of another format takes a live slot's place.
    void replaceSlot (int index, std::shared_ptr<EffectSlot> slot);

private:
    void changed();

    MixerPlanner* planner = nullptr;
    std::vector<std::shared_ptr<EffectSlot>> slots;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InsertChain)
};

} // namespace audium
