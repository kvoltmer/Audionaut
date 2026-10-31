//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "SendList.h"

#include <algorithm>

#include "MixerPlanner.h"

namespace audium
{

std::shared_ptr<SendState> SendList::get (int auxId) const noexcept
{
    for (const auto& entry : entries)
        if (entry.auxId == auxId)
            return entry.state;
    return nullptr;
}

std::shared_ptr<SendState> SendList::getOrAdd (int auxId)
{
    if (auto existing = get (auxId))
        return existing;

    // kept in aux id order, so the JSON and the plan do not depend on
    // the order the sends were made in
    auto state = std::make_shared<SendState>();
    const auto at = std::find_if (entries.begin(), entries.end(), [auxId] (const SendEntry& entry) { return entry.auxId > auxId; });
    entries.insert (at, { auxId, state });
    changed();
    return state;
}

bool SendList::remove (int auxId)
{
    const auto at = std::find_if (entries.begin(), entries.end(), [auxId] (const SendEntry& entry) { return entry.auxId == auxId; });
    if (at == entries.end())
        return false;

    // the state lives on in the plans that reference it until they retire
    entries.erase (at);
    changed();
    return true;
}

void SendList::clear()
{
    if (entries.empty())
        return;
    entries.clear();
    changed();
}

void SendList::writeToJson (json& output, const char* key) const
{
    if (entries.empty())
        return;

    auto sends = json::array();
    for (const auto& entry : entries)
    {
        sends.push_back ({ { "aux", entry.auxId },
                           { "level", entry.state->level.load (std::memory_order_relaxed) },
                           { "pre_fader", entry.state->preFader.load (std::memory_order_relaxed) },
                           { "mute", entry.state->mute.load (std::memory_order_relaxed) } });
    }
    output[key] = std::move (sends);
}

void SendList::readFromJson (const json& input, const char* key)
{
    json sends = json::array();
    if (input.is_object() && input.contains (key) && input[key].is_array())
        sends = input[key];

    // the same state goes on where the aux id is the same, so a value
    // changes without a rebuild; what is left over goes with the old plan
    std::vector<SendEntry> previous = std::move (entries);
    entries.clear();

    for (const auto& element : sends)
    {
        if (! element.is_object() || ! element.contains ("aux") || ! element["aux"].is_number_integer())
            continue;

        const auto auxId = element["aux"].get<int>();
        if (auxId < 0 || get (auxId) != nullptr)
            continue;

        std::shared_ptr<SendState> state;
        for (auto& entry : previous)
        {
            if (entry.auxId == auxId && entry.state != nullptr)
            {
                state = std::move (entry.state);
                break;
            }
        }
        if (state == nullptr)
            state = std::make_shared<SendState>();

        if (element.contains ("level") && element["level"].is_number())
            state->level.store (juce::jmax (0.0f, element["level"].get<float>()), std::memory_order_relaxed);
        if (element.contains ("pre_fader") && element["pre_fader"].is_boolean())
            state->preFader.store (element["pre_fader"].get<bool>(), std::memory_order_relaxed);
        if (element.contains ("mute") && element["mute"].is_boolean())
            state->mute.store (element["mute"].get<bool>(), std::memory_order_relaxed);

        const auto at = std::find_if (entries.begin(), entries.end(), [auxId] (const SendEntry& entry) { return entry.auxId > auxId; });
        entries.insert (at, { auxId, std::move (state) });
    }

    auto same = entries.size() == previous.size();
    for (size_t i = 0; same && i < entries.size(); ++i)
        same = entries[i].auxId == previous[i].auxId && entries[i].state == previous[i].state;
    if (! same)
        changed();
}

void SendList::changed()
{
    if (planner != nullptr)
        planner->requestRebuild();
}

} // namespace audium
