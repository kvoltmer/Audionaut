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
 * The ordered inserts of one track, aux bus or the master: a
 * list of effect slots the mix plan is built from. Every change of the
 * structure (add, remove, move) asks the planner for a new plan; enabling
 * and parameter changes are atomics the running plan picks up. The one
 * exception is a parameter that moves the effect's latency (the
 * compressor's look-ahead): setParameter() sees the slot gone stale and
 * asks for a plan, which prepares the slot again around the new figure.
 *
 * The JSON form is an array of {"type", "enabled", "state"} under the key
 * "inserts", written only when there is something in it, so a project
 * without effects serializes as before. readFromJson keeps the effect
 * instances where the types line up, so an undo step or a hosted edit that
 * changes a value does not rebuild the chain. An element of a type this
 * engine cannot create (a project from a newer build) is kept as it was
 * read and written back in its place, so saving the project does not lose
 * it; it is not a slot and nothing runs for it. A hosted plugin's element
 * carries a "plugin" block beside the type (name, manufacturer, format,
 * file), and an element whose type id this machine lacks may be read with
 * the same plugin in another format - the planner asks the registry.
 *
 * A hosted plugin can move its own latency (a look-ahead switched on in its
 * own window): the chain hears of it through Effect::onLatencyChanged and
 * asks for a plan, as setParameter() does for a built-in.
 */
class InsertChain
{
public:
    explicit InsertChain (MixerPlanner* planner_ = nullptr) : planner (planner_) {}
    ~InsertChain();

    /// An element read from JSON that no slot could be made for, kept for the file.
    struct Unresolved
    {
        int index;      // position in the array it was read from
        json element;
    };

    /// The planner that creates effects and rebuilds the plan; nullptr makes the chain inert (tests without one).
    void setPlanner (MixerPlanner* planner_) noexcept { planner = planner_; }

    int size() const noexcept { return static_cast<int> (slots.size()); }
    bool isEmpty() const noexcept { return slots.empty(); }
    /// Elements read from JSON that no slot could be made for; written back as they were.
    int getNumUnresolved() const noexcept { return static_cast<int> (unresolved.size()); }
    const std::vector<Unresolved>& getUnresolved() const noexcept { return unresolved; }
    /// Drops the unresolved element at the position in that list (not a slot index); false out of range.
    bool removeUnresolved (int position);
    const std::vector<std::shared_ptr<EffectSlot>>& getSlots() const noexcept { return slots; }

    /// The slot at the index, nullptr out of range.
    std::shared_ptr<EffectSlot> getSlot (int index) const noexcept;

    /// Adds an effect of the type at the index (-1 = the end); nullptr for an unknown type or without a planner.
    std::shared_ptr<EffectSlot> add (const std::string& typeId, int at = -1);

    bool remove (int index);
    bool move (int from, int to);
    bool setEnabled (int index, bool enabled);
    void clear();

    /// Stores the value into the slot's effect; where that changed the effect's latency, a plan is requested. False out of range.
    bool setParameter (int index, int parameter, float value);

    /// Writes the chain under `key` when it has slots or unresolved elements; a project without effects serializes as before.
    void writeToJson (json& output, const char* key = "inserts") const;
    /// Reads the chain under `key` (an absent key empties it), keeping instances where the types line up.
    void readFromJson (const json& input, const char* key = "inserts");

private:
    void changed();
    void adopt (EffectSlot& slot);     // the slot's effect reports latency moves to this chain
    void release (EffectSlot& slot);   // ... until it leaves

    MixerPlanner* planner = nullptr;
    std::vector<std::shared_ptr<EffectSlot>> slots;
    std::vector<Unresolved> unresolved;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InsertChain)
};

} // namespace audium
