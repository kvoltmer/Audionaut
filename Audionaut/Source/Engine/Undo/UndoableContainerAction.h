//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/Streamable.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Playback/AudioBusInterface.h"

namespace audium
{

/**
 * @struct UndoableContainerAction
 * @brief Represents an undoable action for an `AudioTrackContainer`.
 *
 * The `UndoableContainerAction` struct provides functionality to store the state of an
 * `AudioTrackContainer` and perform or undo changes to it. It uses JUCE's `UndoableAction`
 * as its base class.
 */
struct UndoableContainerAction final : public juce::UndoableAction
{
    /**
     * @brief Constructs an `UndoableContainerAction`.
     * @param container_ A reference to the `AudioTrackContainer` to manage.
     * @param rebuild_ A boolean indicating whether to rebuild the container state after undo/redo.
     */
    UndoableContainerAction (AudioTrackContainer &container_, bool rebuild_ = true) noexcept :
        container (container_),
        rebuild (rebuild_)
    {
        storeOldState();
    }
    
    /**
     * @brief Destructor for `UndoableContainerAction`.
     */
    ~UndoableContainerAction() override
    {
        //std::cout << "~UndoableContainerAction" << std::endl;
    }
    
    /**
     * @brief Stores the current state of the container as the "old" state.
     */
    void storeOldState()
    {
        juce::MemoryOutputStream outStream;
        container.writeToStream(outStream);
        oldMemoryBlock = outStream.getMemoryBlock();
    }
    
    /**
     * @brief Stores the current state of the container as the "new" state.
     */
    void storeNewState()
    {
        juce::MemoryOutputStream outStream;
        container.writeToStream(outStream);
        newMemoryBlock = outStream.getMemoryBlock();
    }
    
    /**
     * @brief Performs the action by applying the "new" state to the container.
     * @return True if the action was successfully performed, false otherwise.
     */
    bool perform() override
    {
        try {
            juce::MemoryInputStream inputStream(newMemoryBlock, false);
            container.readFromStream(inputStream, rebuild);
        }
        catch (std::exception &e) {
            std::cout << "UndoableContainerAction::perform -> " << e.what() << std::endl;
            rollBackTo(oldMemoryBlock);
            return false;
        }
        return true;
    }

    /**
     * @brief Undoes the action by restoring the "old" state to the container.
     * @return True if the action was successfully undone, false otherwise.
     */
    bool undo() override
    {
        try {

            if (container.audioBusInterface->anyChannelRecording())
                container.audioBusInterface->record(false);

            juce::MemoryInputStream inputStream(oldMemoryBlock, false);
            container.readFromStream(inputStream, rebuild);
        }
        catch (std::exception &e) {
            std::cout << "UndoableContainerAction::undo -> " << e.what() << std::endl;
            rollBackTo(newMemoryBlock);
            return false;
        }
        return true;
    }

    /**
     * @brief Best-effort restore after a failed apply, so a rejected state
     *        doesn't leave a partially mutated container behind.
     */
    void rollBackTo (const juce::MemoryBlock& state)
    {
        try {
            juce::MemoryInputStream inputStream(state, false);
            container.readFromStream(inputStream, rebuild);
        }
        catch (std::exception &e) {
            std::cout << "UndoableContainerAction: rollback failed -> " << e.what() << std::endl;
        }
    }

    /**
     * @brief Retrieves the size of the action in units (KB of stored state,
     *        cached on first call so insert and removal see the same value).
     * @return The size of the action in units.
     */
    int getSizeInUnits() override
    {
        // A constant, size-proportional value: the live container's track
        // count would hide the real memory cost of the two stored snapshots
        // from the UndoManager's trimming budget, and drift between insert
        // and removal, corrupting its unit accounting.
        if (sizeInUnits == 0)
            sizeInUnits = (int) juce::jmax ((size_t) 1,
                                            (oldMemoryBlock.getSize() + newMemoryBlock.getSize()) / 1024);
        return sizeInUnits;
    }
    
    /**
      * @brief A reference to the `AudioTrackContainer` being managed.
      */
     AudioTrackContainer &container;

     /**
      * @brief The memory block storing the "old" state of the container.
      */
     juce::MemoryBlock oldMemoryBlock;

     /**
      * @brief The memory block storing the "new" state of the container.
      */
     juce::MemoryBlock newMemoryBlock;

     /**
      * @brief A boolean indicating whether to rebuild the container state after undo/redo.
      */
     bool rebuild = true;

     /**
      * @brief Cached size of both stored snapshots in KB (0 = not yet computed).
      */
     int sizeInUnits = 0;

     /**
      * @brief JUCE macro to prevent copying and detect memory leaks.
      */
     JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndoableContainerAction)
};

} // namespace audium
