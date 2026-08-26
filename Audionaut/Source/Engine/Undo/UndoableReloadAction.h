//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
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
 */
struct UndoableReloadAction final : public juce::UndoableAction
{
    /**
     * @brief Constructs an `UndoableReloadAction`.
     * @param engine_ The engine to apply states to.
     * @param beforeState_ Full project JSON of the pre-reload in-memory state.
     * @param afterState_ Full project JSON of the state to apply.
     * @param preserveUiState_ True to keep the in-memory UI state in both
     *        directions (external reload); false to adopt each snapshot's own
     *        UI state (autosave restore).
     * @param marksExternalChange_ True when the action applies an external
     *        (agent/CLI) change: perform/redo set the engine's external-change
     *        marker, undo clears it.
     */
    UndoableReloadAction (AudiumEngine& engine_, json beforeState_, json afterState_,
                          bool preserveUiState_ = true, bool marksExternalChange_ = false) noexcept :
        engine (engine_),
        beforeState (std::move(beforeState_)),
        afterState (std::move(afterState_)),
        preserveUiState (preserveUiState_),
        marksExternalChange (marksExternalChange_)
    {
    }

    /**
     * @brief Applies the "after" state to the engine.
     * @return True if the state was successfully applied, false otherwise.
     */
    bool perform() override
    {
        try {
            const auto ok = engine.applyProjectJson(afterState, preserveUiState);
            if (ok && marksExternalChange)
                engine.setChangedExternally(true);
            return ok;
        }
        catch (std::exception &e) {
            std::cout << "UndoableReloadAction::perform -> " << e.what() << std::endl;
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
            if (ok && marksExternalChange)
                engine.setChangedExternally(false);
            return ok;
        }
        catch (std::exception &e) {
            std::cout << "UndoableReloadAction::undo -> " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Retrieves the size of the action in units.
     * @return The size of the action in units.
     */
    int getSizeInUnits() override    { return engine.getSizeInUnits(); }

    /**
     * @brief The engine being managed.
     */
    AudiumEngine &engine;

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
     * @brief Whether this action moves the engine's external-change marker.
     */
    bool marksExternalChange = false;

    /**
     * @brief JUCE macro to prevent copying and detect memory leaks.
     */
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndoableReloadAction)
};

} // namespace audium
