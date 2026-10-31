//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AuxBusContainer.h"

#include <algorithm>

#include "MixerPlanner.h"

namespace audium
{

void AuxBusContainer::setPlanner (MixerPlanner* planner_) noexcept
{
    planner = planner_;
    for (auto& bus : buses)
        bus->setPlanner (planner);
}

std::shared_ptr<AuxBus> AuxBusContainer::get (int id) const noexcept
{
    for (const auto& bus : buses)
        if (bus->getId() == id)
            return bus;
    return nullptr;
}

std::shared_ptr<AuxBus> AuxBusContainer::getAt (int index) const noexcept
{
    return index >= 0 && index < size() ? buses[static_cast<size_t> (index)] : nullptr;
}

int AuxBusContainer::indexOf (int id) const noexcept
{
    for (size_t i = 0; i < buses.size(); ++i)
        if (buses[i]->getId() == id)
            return static_cast<int> (i);
    return -1;
}

std::shared_ptr<AuxBus> AuxBusContainer::make (int id, std::string name)
{
    if (name.empty())
        name = "Aux " + std::to_string (id);
    nextId = std::max (nextId, id + 1);
    return std::make_shared<AuxBus> (id, std::move (name), planner);
}

std::shared_ptr<AuxBus> AuxBusContainer::add (std::string name)
{
    auto bus = make (nextId, std::move (name));
    buses.push_back (bus);
    changed();
    return bus;
}

bool AuxBusContainer::remove (int id)
{
    const auto at = std::find_if (buses.begin(), buses.end(), [id] (const auto& bus) { return bus->getId() == id; });
    if (at == buses.end())
        return false;

    // the bus's state and slots live on in the plans that reference them
    // until those retire
    buses.erase (at);
    changed();
    return true;
}

void AuxBusContainer::resetToDefaults()
{
    buses.clear();
    nextId = 1;
    for (auto i = 0; i < numDefaultBuses; ++i)
        buses.push_back (make (nextId, {}));
    changed();
}

void AuxBusContainer::clear()
{
    nextId = 1;
    if (buses.empty())
        return;
    buses.clear();
    changed();
}

void AuxBusContainer::writeToJson (json& output, const char* key) const
{
    auto array = json::array();
    for (const auto& bus : buses)
    {
        json element;
        bus->writeToJson (element);
        array.push_back (std::move (element));
    }
    output[key] = std::move (array);
}

void AuxBusContainer::readFromJson (const json& input, const char* key)
{
    if (! input.is_object() || ! input.contains (key) || ! input[key].is_array())
    {
        // a project from before there were aux buses: it gets the two
        // every project has
        resetToDefaults();
        return;
    }

    // the same bus goes on where the id is the same, in the file's order,
    // so a value changes without a rebuild; what is left over goes with
    // the old plan
    std::vector<std::shared_ptr<AuxBus>> previous = std::move (buses);
    std::vector<int> previousIds;
    for (const auto& bus : previous)
        previousIds.push_back (bus->getId());
    buses.clear();
    nextId = 1;

    for (const auto& element : input[key])
    {
        if (! element.is_object() || ! element.contains ("id") || ! element["id"].is_number_integer())
            continue;

        const auto id = element["id"].get<int>();
        if (id < 1 || get (id) != nullptr)
            continue;

        std::shared_ptr<AuxBus> bus;
        for (auto& candidate : previous)
        {
            if (candidate != nullptr && candidate->getId() == id)
            {
                bus = std::move (candidate);
                break;
            }
        }
        if (bus == nullptr)
            bus = make (id, {});
        else
            nextId = std::max (nextId, id + 1);

        bus->readFromJson (element);
        buses.push_back (std::move (bus));
    }

    // the structure is the same when every previous bus was taken over,
    // at the place it had
    auto same = buses.size() == previous.size();
    for (size_t i = 0; same && i < buses.size(); ++i)
        same = previous[i] == nullptr && buses[i]->getId() == previousIds[i];
    if (! same)
        changed();
}

void AuxBusContainer::changed()
{
    if (planner != nullptr)
        planner->requestRebuild();
}

} // namespace audium
