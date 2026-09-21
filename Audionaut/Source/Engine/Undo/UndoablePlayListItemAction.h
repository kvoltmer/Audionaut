//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/ActionMessages.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"

namespace audium
{

/**
 * @struct UndoablePlayListItemAction
 * @brief Undo step for a clip drag: position, trim (region window) and
 *        speed of the dragged clips.
 *
 * Snapshots only the timeline state of the affected `PlayListItem`s instead
 * of the whole track container. The container snapshot's replay re-read
 * every clip from JSON, and `PlayListItem::readFromJson` calls `init()`,
 * which tears down and recreates each clip's voice sources (including a
 * Rubber Band stretcher per clip) - a DSP spike on every clip drop, with
 * playing clips restarting. Applying this step only writes the stored
 * values back through the item's setters and re-sorts its playlist, so the
 * playback chain is untouched.
 *
 * Items are held weakly: a step whose items were replaced by a rebuilding
 * replay (track added or removed) fails to apply, which makes the undo
 * manager drop its history instead of touching the wrong clips.
 */
struct UndoablePlayListItemAction final : public juce::UndoableAction
{
    /** The per-clip state a drag can change. */
    struct State
    {
        double positionClocks = 0.0;
        juce::Range<double> regionClocks;
        double speedRatio = 1.0;
        bool tempoLocked = false;
        double clipTempo = 0.0;

        bool operator== (const State&) const = default;
    };

    UndoablePlayListItemAction (AudioTrackContainer& container_,
                                const std::vector<std::shared_ptr<PlayListItem>>& items)
        : container (container_)
    {
        for (auto& item : items)
            if (item != nullptr)
                entries.push_back ({ item, capture (*item), capture (*item) });
    }

    /** Captures the items' current state as the state perform() applies. */
    void storeNewState()
    {
        for (auto& entry : entries)
            if (auto item = entry.item.lock())
                entry.newState = capture (*item);
    }

    /** True when nothing changed between the two stored states. */
    bool isNoOp() const noexcept
    {
        return std::all_of (entries.begin(), entries.end(),
                            [] (const Entry& e) { return e.oldState == e.newState; });
    }

    bool perform() override
    {
        return apply (true);
    }

    bool undo() override
    {
        return apply (false);
    }

    int getSizeInUnits() override
    {
        return juce::jmax (1, (int) entries.size());
    }

    static State capture (const PlayListItem& item)
    {
        return { item.getAbsolutePosition (audium::clocks),
                 item.getRegionData (audium::clocks),
                 item.getSpeedRatio(),
                 item.isTempoLocked(),
                 item.getClipTempo() };
    }

private:
    struct Entry
    {
        std::weak_ptr<PlayListItem> item;
        State oldState;
        State newState;
    };

    static void restore (PlayListItem& item, const State& state)
    {
        // order matters (see PlayListItem::copySpeedFrom): the plain ratio
        // only takes while the clip is unlocked
        item.setTempoLocked (false);
        item.setSpeedRatio (state.speedRatio);
        item.setClipTempo (state.clipTempo);
        item.setTempoLocked (state.tempoLocked);

        item.setRegionData (state.regionClocks, audium::clocks);
        item.setAbsolutePosition (state.positionClocks, audium::clocks);
    }

    bool apply (bool toNewState)
    {
        std::vector<std::shared_ptr<PlayListItem>> items;
        items.reserve (entries.size());

        // resolve everything first: an expired item means the clips were
        // rebuilt since, and a half-applied step would be worse than none
        for (auto& entry : entries)
        {
            auto item = entry.item.lock();
            if (item == nullptr)
                return false;
            items.push_back (item);
        }

        std::vector<PlayListContainer*> playLists;
        for (size_t i = 0; i < entries.size(); ++i)
        {
            restore (*items[i], toNewState ? entries[i].newState : entries[i].oldState);

            auto* playList = &items[i]->getPlayListContainer();
            if (std::find (playLists.begin(), playLists.end(), playList) == playLists.end())
                playLists.push_back (playList);
        }

        // a move can change the order the scheduler walks
        for (auto* playList : playLists)
            playList->sortByPosition();

        container.sendActionMessage (updateArrangementAction);
        return true;
    }

    AudioTrackContainer& container;
    std::vector<Entry> entries;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoablePlayListItemAction)
};

} // namespace audium
