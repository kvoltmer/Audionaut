//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>

/**
 * What the mixer asks of an editor window's content, whichever editor it
 * is (the dial grid of a built-in, a plugin's own): whether the slot it
 * edits is still in the project, and a way to say when it is not.
 */
class SlotEditor
{
public:
    virtual ~SlotEditor() = default;

    /// The slot is still in its chain and the chain still in the project.
    virtual bool isStillValid() const = 0;

    /// Asked when the slot went away; the owner closes the window (asynchronously).
    std::function<void()> onSlotGone;
};
