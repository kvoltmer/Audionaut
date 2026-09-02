//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <memory>
#include <JuceHeader.h>

#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"

namespace audium {

/**
 * Runs @p mutate against the track container as one undoable transaction.
 *
 * The container is snapshotted before and after, so however many objects
 * @p mutate creates, moves or deletes, a single Undo takes all of it back.
 * When @p mutate returns false nothing is recorded and the container is
 * assumed untouched.
 *
 * Message thread only, like every other container mutation.
 */
inline bool applyAsUndoableEdit (AudioTrackContainer& container,
                                 const std::function<bool()>& mutate,
                                 const juce::String& transactionName)
{
    auto action = std::make_unique<UndoableContainerAction> (container);

    if (! mutate())
        return false;

    action->storeNewState();
    container.getUndoManager()->perform (action.release(), transactionName);
    container.getUndoManager()->beginNewTransaction();

    return true;
}

} // namespace audium
