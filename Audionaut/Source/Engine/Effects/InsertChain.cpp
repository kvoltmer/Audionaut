//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "InsertChain.h"

#include "MixerPlanner.h"

namespace audium
{

std::shared_ptr<EffectSlot> InsertChain::getSlot (int index) const noexcept
{
    return index >= 0 && index < size() ? slots[static_cast<size_t> (index)] : nullptr;
}

std::shared_ptr<EffectSlot> InsertChain::add (const std::string& typeId, int at)
{
    if (planner == nullptr)
        return nullptr;

    auto slot = planner->createSlot (typeId);
    if (slot == nullptr)
        return nullptr;

    const auto index = at < 0 || at > size() ? size() : at;
    slots.insert (slots.begin() + index, slot);
    changed();
    return slot;
}

bool InsertChain::remove (int index)
{
    if (index < 0 || index >= size())
        return false;

    // the slot lives on in the plans that reference it until they retire
    slots.erase (slots.begin() + index);
    changed();
    return true;
}

bool InsertChain::move (int from, int to)
{
    if (from < 0 || from >= size() || to < 0 || to >= size())
        return false;
    if (from == to)
        return true;

    auto slot = slots[static_cast<size_t> (from)];
    slots.erase (slots.begin() + from);
    slots.insert (slots.begin() + to, slot);
    changed();
    return true;
}

bool InsertChain::setEnabled (int index, bool enabled)
{
    if (auto slot = getSlot (index))
    {
        slot->setEnabled (enabled);   // an atomic the running plan reads: no rebuild
        return true;
    }
    return false;
}

void InsertChain::clear()
{
    if (slots.empty())
        return;
    slots.clear();
    changed();
}

void InsertChain::replaceSlot (int index, std::shared_ptr<EffectSlot> slot)
{
    if (index >= 0 && index < size() && slot != nullptr)
        slots[static_cast<size_t> (index)] = std::move (slot);
}

void InsertChain::writeToJson (json& output, const char* key) const
{
    if (slots.empty())
        return;

    auto inserts = json::array();
    for (const auto& slot : slots)
    {
        inserts.push_back ({ { "type", slot->getEffect().getType().id },
                             { "enabled", slot->isEnabled() },
                             { "state", slot->getEffect().getState() } });
    }
    output[key] = std::move (inserts);
}

void InsertChain::readFromJson (const json& input, const char* key)
{
    json inserts = json::array();
    if (input.is_object() && input.contains (key) && input[key].is_array())
        inserts = input[key];

    // the same instance goes on where the type is the same, in order, so
    // values change without a rebuild and an insert or removal in the
    // middle keeps the others; what is left over goes with the old plan
    std::vector<std::shared_ptr<EffectSlot>> previous = std::move (slots);
    std::vector<bool> used (previous.size(), false);
    slots.clear();

    for (const auto& element : inserts)
    {
        if (! element.is_object() || ! element.contains ("type"))
            continue;

        const auto typeId = element["type"].get<std::string>();
        std::shared_ptr<EffectSlot> slot;

        for (size_t i = 0; i < previous.size(); ++i)
        {
            if (! used[i] && previous[i]->getEffect().getType().id == typeId)
            {
                used[i] = true;
                slot = previous[i];
                break;
            }
        }

        if (slot == nullptr)
        {
            if (planner == nullptr)
                continue;
            slot = planner->createSlot (typeId);
            if (slot == nullptr)
                continue;   // a type this engine cannot create: dropped, like a missing plugin would be
        }

        if (element.contains ("enabled") && element["enabled"].is_boolean())
            slot->setEnabled (element["enabled"].get<bool>());
        if (element.contains ("state"))
            slot->getEffect().setState (element["state"]);

        slots.push_back (std::move (slot));
    }

    if (slots != previous)
        changed();
}

void InsertChain::changed()
{
    if (planner != nullptr)
        planner->requestRebuild();
}

} // namespace audium
