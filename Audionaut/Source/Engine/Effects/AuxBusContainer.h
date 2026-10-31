//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <JuceHeader.h>

#include "AuxBus.h"

namespace audium
{

class MixerPlanner;

/**
 * The aux buses of a project, in strip order, owned by the track
 * container. Ids are stable for the life of the project: a new bus takes
 * the next free one, a removed id is never reused, so a send keeps pointing
 * at the bus it was made for across edits and undo.
 *
 * A project starts with two buses, "Aux 1" (id 1) and "Aux 2" (id 2), like
 * the aux sends of a console: resetToDefaults() makes them for a new
 * project, and a project file without an "aux_buses" key (one saved before
 * there were any) gets them on load. An empty array in the file means the
 * user removed them, and it stays empty.
 *
 * Adding and removing buses change the plan's structure and ask the planner
 * for a new one; a bus's values are atomics the running plan reads.
 */
class AuxBusContainer
{
public:
    static constexpr int numDefaultBuses = 2;

    /// Starts with the two default buses (no planner yet: setPlanner() reaches their chains).
    explicit AuxBusContainer (MixerPlanner* planner_ = nullptr) : planner (planner_) { resetToDefaults(); }

    /// The planner the buses' chains create their effects through; nullptr makes the container inert (tests without one).
    void setPlanner (MixerPlanner* planner_) noexcept;

    int size() const noexcept { return static_cast<int> (buses.size()); }
    bool isEmpty() const noexcept { return buses.empty(); }
    const std::vector<std::shared_ptr<AuxBus>>& getBuses() const noexcept { return buses; }

    /// The bus with the id, nullptr for none.
    std::shared_ptr<AuxBus> get (int id) const noexcept;
    /// The bus at the strip index, nullptr out of range.
    std::shared_ptr<AuxBus> getAt (int index) const noexcept;
    /// The strip index of the bus with the id, -1 for none.
    int indexOf (int id) const noexcept;

    /// A new bus at the end, named "Aux <id>" when no name is given.
    std::shared_ptr<AuxBus> add (std::string name = {});
    bool remove (int id);

    /// Two buses, "Aux 1" and "Aux 2", and nothing else; ids start over (a new project).
    void resetToDefaults();
    void clear();

    /// Always written, so a project with its buses removed stays that way.
    void writeToJson (json& output, const char* key = "aux_buses") const;
    /// Reads in place by id (instances kept where the ids line up); an absent key gives the defaults.
    void readFromJson (const json& input, const char* key = "aux_buses");

private:
    void changed();
    std::shared_ptr<AuxBus> make (int id, std::string name);

    MixerPlanner* planner = nullptr;
    std::vector<std::shared_ptr<AuxBus>> buses;
    int nextId = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuxBusContainer)
};

} // namespace audium
