//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace audium
{

class MixerPlanner;

/**
 * The values of one send, read by the render thread per block: the level
 * (linear gain, like a channel's), whether the send taps before the fader
 * (then it ignores the channel's gain, mute and solo) and whether it is
 * muted. Atomics, so a change needs no new plan; the plan holds a
 * shared_ptr to the state, so a model rebuild under a live plan cannot
 * free it.
 */
struct SendState
{
    std::atomic<float> level { 0.0f };
    std::atomic<bool> preFader { false };
    std::atomic<bool> mute { false };
};

/// One send of a bus channel: into which aux bus, with what state.
struct SendEntry
{
    int auxId = -1;
    std::shared_ptr<SendState> state;
};

/**
 * The sends of one bus channel, one per aux bus at most. A bus channel
 * without an entry for an aux sends nothing to it; getOrAdd() creates the
 * entry (at level 0) the first time a send is set. Adding and removing
 * entries change the plan's structure and ask the planner for a new one;
 * the values are atomics the running plan reads.
 *
 * JSON: an array of {"aux", "level", "pre_fader", "mute"} under "sends",
 * written only when there is an entry, so a project without sends
 * serializes as before. readFromJson keeps the state instances where the
 * aux ids line up, so an undo step that changes a level does not rebuild.
 */
class SendList
{
public:
    explicit SendList (MixerPlanner* planner_ = nullptr) : planner (planner_) {}

    void setPlanner (MixerPlanner* planner_) noexcept { planner = planner_; }

    int size() const noexcept { return static_cast<int> (entries.size()); }
    bool isEmpty() const noexcept { return entries.empty(); }
    const std::vector<SendEntry>& getEntries() const noexcept { return entries; }

    /// The send into the aux, nullptr when there is none.
    std::shared_ptr<SendState> get (int auxId) const noexcept;

    /// The send into the aux, created at level 0 if there was none (a new plan then).
    std::shared_ptr<SendState> getOrAdd (int auxId);

    bool remove (int auxId);
    void clear();

    void writeToJson (json& output, const char* key = "sends") const;
    void readFromJson (const json& input, const char* key = "sends");

private:
    void changed();

    MixerPlanner* planner = nullptr;
    std::vector<SendEntry> entries;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SendList)
};

} // namespace audium
