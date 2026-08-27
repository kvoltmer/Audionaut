//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/ProjectFileStore.h"
#include "Engine/Playback/AudioBusInterface.h"

namespace audium
{

/**
 * @struct UndoableReloadAction
 * @brief Applies a full project state to the engine as an undoable action.
 *
 * Used when the open project changed on disk (e.g. edited by an agent through
 * the CLI/MCP server) and the running app reloads it: `perform()` applies the
 * on-disk state, `undo()` restores the pre-reload in-memory state. Neither
 * direction touches the file on disk - undoing a reload leaves the external
 * writer's version in the project package.
 *
 * The store's external-change marker and authoritative `currentJson` follow
 * the applied state through perform/undo/redo.
 */
struct UndoableReloadAction final : public juce::UndoableAction
{
    /**
     * @brief Constructs an `UndoableReloadAction`.
     * @param engine_ The engine to apply states to.
     * @param store_ The store owning the session state (marker, currentJson).
     * @param beforeState_ Full project JSON of the pre-reload in-memory state.
     * @param afterState_ Full project JSON of the state to apply.
     * @param preserveUiState_ True to keep the in-memory UI state in both
     *        directions (external reload); false to adopt each snapshot's own
     *        UI state (autosave restore).
     * @param marksExternalChange_ True when the action applies an external
     *        (agent/CLI) change: perform/redo set the store's external-change
     *        marker, undo restores its prior value.
     */
    UndoableReloadAction (AudiumEngine& engine_, ProjectFileStore& store_,
                          json beforeState_, json afterState_,
                          bool preserveUiState_ = true, bool marksExternalChange_ = false) noexcept :
        engine (engine_),
        store (store_),
        beforeState (std::move(beforeState_)),
        afterState (std::move(afterState_)),
        preserveUiState (preserveUiState_),
        marksExternalChange (marksExternalChange_),
        markerBeforeReload (store_.wasChangedExternally())
    {
        // A constant, size-proportional value (KB of stored JSON): the live
        // engine's track count would both hide the real memory cost from the
        // UndoManager's trimming budget and drift between insert and removal,
        // corrupting its unit accounting.
        sizeInUnits = (int) juce::jmax ((size_t) 1,
                                        (beforeState.dump().size() + afterState.dump().size()) / 1024);
    }

    /**
     * @brief Applies the "after" state to the engine.
     * @return True if the state was successfully applied, false otherwise.
     */
    bool perform() override
    {
        try {
            // an agent write can land mid-take - never swap the project
            // structure underneath live recorders (mirrors undo())
            if (engine.getAudioBusInterface()->anyChannelRecording())
                engine.getAudioBusInterface()->record(false);

            const auto ok = engine.applyProjectJson(afterState, preserveUiState);
            if (ok) {
                store.setCurrentJson(afterState);
                if (marksExternalChange)
                    store.setChangedExternally(true);
            }
            else {
                rollBackTo(beforeState);
            }
            return ok;
        }
        catch (std::exception &e) {
            std::cout << "UndoableReloadAction::perform -> " << e.what() << std::endl;
            rollBackTo(beforeState);
            return false;
        }
    }

    /**
     * @brief Restores the "before" state to the engine.
     * @return True if the state was successfully restored, false otherwise.
     */
    bool undo() override
    {
        try {
            if (engine.getAudioBusInterface()->anyChannelRecording())
                engine.getAudioBusInterface()->record(false);

            const auto ok = engine.applyProjectJson(beforeState, preserveUiState);

            if (ok) {
                store.setCurrentJson(beforeState);

                // the before-state may itself stem from an earlier external
                // reload - restore the marker's prior value, don't force it off
                if (marksExternalChange)
                    store.setChangedExternally(markerBeforeReload);
            }
            else {
                rollBackTo(afterState);
            }
            return ok;
        }
        catch (std::exception &e) {
            std::cout << "UndoableReloadAction::undo -> " << e.what() << std::endl;
            rollBackTo(afterState);
            return false;
        }
    }

    /**
     * @brief Best-effort restore after a failed apply, so a rejected state
     *        doesn't leave a partially mutated session behind.
     */
    void rollBackTo (json& state)
    {
        try {
            if (engine.applyProjectJson(state, preserveUiState))
                store.setCurrentJson(state);
        }
        catch (std::exception &e) {
            std::cout << "UndoableReloadAction: rollback failed -> " << e.what() << std::endl;
        }
    }

    /**
     * @brief Retrieves the size of the action in units (KB of stored JSON,
     *        captured at construction).
     * @return The size of the action in units.
     */
    int getSizeInUnits() override    { return sizeInUnits; }

    /**
     * @brief The engine being managed.
     */
    AudiumEngine &engine;

    /**
     * @brief The store owning the persistence session state.
     */
    ProjectFileStore &store;

    /**
     * @brief Full project JSON of the pre-reload in-memory state.
     */
    json beforeState;

    /**
     * @brief Full project JSON of the state applied by the reload.
     */
    json afterState;

    /**
     * @brief Whether the in-memory UI state survives both directions.
     */
    bool preserveUiState = true;

    /**
     * @brief Whether this action moves the store's external-change marker.
     */
    bool marksExternalChange = false;

    /**
     * @brief The marker's value when this action was created - what undo
     *        restores it to.
     */
    bool markerBeforeReload = false;

    /**
     * @brief Constant size of both stored JSON states in KB.
     */
    int sizeInUnits = 1;

    /**
     * @brief JUCE macro to prevent copying and detect memory leaks.
     */
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndoableReloadAction)
};

} // namespace audium
