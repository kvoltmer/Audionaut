//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include <atomic>

#include "Engine/Core/LockFreeContainer.h"
#include "Engine/PlayList/LoopData.h"
#include "Engine/TimeContext.h"

namespace audium {

class TempoProvider;
class AudioTrackContainer;

/**
 * @class TransportLoop
 * @brief Manages loop playback functionality within the transport system.
 *
 * The `TransportLoop` class provides methods to configure and control loop playback,
 * including setting loop ranges, activating or deactivating loops, and processing
 * looped playback. It integrates with the undo system and tempo provider for
 * seamless audio editing and playback.
 */
class TransportLoop : private juce::AsyncUpdater {
public:
    /**
     * @brief Constructs a `TransportLoop` instance.
     * @param undoManager_ Shared pointer to the `juce::UndoManager` for managing undo/redo operations.
     */
    TransportLoop(std::shared_ptr<juce::UndoManager> undoManager_,
                  std::shared_ptr<TempoProvider> tempoProvider_) :
         undoManager(undoManager_),
         tempoProvider(tempoProvider_)
     {
         publishLoopData();
     }

    /**
     * @brief Destructor for `TransportLoop`.
     */
    ~TransportLoop() override
    {
        cancelPendingUpdate();
    }

    /**
     * @brief Prepares the transport loop for audio playback.
     * @param samplesPerBlockExpected The expected number of samples per block.
     * @param sampleRate The sample rate for audio processing.
     */
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate);

    /**
     * @brief Sets the loop position range.
     * @param audioTrackContainer Shared pointer to the `AudioTrackContainer` for track data.
     * @param newRange The new loop range as a `juce::Range<double>`.
     * @param context The time context type for the loop range.
     */
    void setLoopPositionRange(std::shared_ptr<AudioTrackContainer> audioTrackContainer,
                              juce::Range<double> newRange,
                              audium::TimeContextType context);

    /**
     * @brief Gets the current loop position range.
     * @param context The time context type for the loop range.
     * @return The current loop range as a `juce::Range<double>`.
     */
    juce::Range<double> getLoopPositionRange(audium::TimeContextType context) const;

    /**
     * @brief Checks if the loop is currently active.
     * @return True if the loop is active, false otherwise.
     */
    bool isLoopActive() const;

    /**
     * @brief Activates or deactivates the loop.
     * @param bActive True to activate the loop, false to deactivate.
     */
    void setLoopActive(bool bActive);

    /**
     * @brief Gets the loop data as the message thread holds it (persistence).
     */
    LoopData getLoopData() const noexcept { return loopData; }

    /**
     * @brief Replaces the loop data as a whole (project load, undo) and
     *        publishes it to the audio thread.
     */
    void setLoopData(const LoopData& newData);


    struct LoopResult {
        bool loopEvent          = false;
        double positionResult   = 0.0;
        double timeUntilLoop    = 0.0;
        int numSamplesUntilLoop = 0;
        TimeContextType context = seconds;

        /// Samples from this block's start to the loop end while playing
        /// inside the loop (>= numSamples when no loop event fires in this
        /// block); -1 when the loop is off or the position is outside it.
        int numSamplesUntilLoopEnd = -1;
    };
    
    /**
     * @brief Processes the loop during playback.
     * @param thePosition Reference to the current playback position.
     * @param numSamples The number of samples to process.
     * @return True if the loop was processed, false otherwise.
     */
    const LoopResult processLoop(double thePosition,
                                 int numSamples,
                                 audium::TimeContextType context);

    /**
     * @brief Resets the loop state to its default configuration.
     */
    void reset();
    
    void setAbsoluteStartPosition(double newPosition, audium::TimeContextType context);
    
    int getLoopCount() const noexcept { return loopCount; }
        
    bool isWithinLoop() const noexcept { return withinLoop; }
    
    double getLoopPhaseForPosition(double startPosition,
                                double length,
                                audium::TimeContextType context) const;
    
    double getCurrentPosition(audium::TimeContextType context) const noexcept;

    /**
     * @brief The loop range the last processLoop() applied (audio thread).
     *
     * The scheduler reads it in the same block, so it never differs from
     * the range that block's loop result was computed with; a test uses it
     * to check that a range change arrives whole.
     */
    juce::Range<double> getProcessedLoopPositionRange(audium::TimeContextType context) const;


    std::function<void()> onLoopEnteredFunction;
    
    std::function<void()> onLoopActionFunction;
    
    std::function<void()> onPlayListItemUpdateFunction;

private:
    std::shared_ptr<juce::UndoManager> undoManager; ///< Undo manager for loop-related operations.
    std::shared_ptr<TempoProvider> tempoProvider; ///< Tempo provider for tempo-based calculations.

    /// The message thread's loop data: what the setters change and the
    /// getters report. The audio thread never reads it directly - a range
    /// or the active flag changing between two of its reads in one block
    /// (loop selection, a range drag, project load, undo) would wrap or
    /// seek at a place nobody chose. Every change goes out whole through
    /// the triple buffer instead, and processLoop() pulls the latest
    /// snapshot once per block into processedLoopData, which is all the
    /// audio thread uses for that block.
    LoopData loopData;
    LockFreeContainer<LoopData> publishedLoopData { 1 };
    LoopData processedLoopData; ///< Audio thread only.

    /// Message thread: hands the current loopData to the audio thread.
    void publishLoopData();

    /// Audio thread: takes over the latest published loop data, if any.
    void pullLoopData();

    juce::Range<double> loopRangeOf(const LoopData& data, audium::TimeContextType context) const;

    double externalSampleRate = 44100.0; ///< The external sample rate for playback.
    int loopCount = 0; ///< Counter for the number of loop iterations.
    bool withinLoop = false; ///< Flag indicating whether playback is within the loop range.
    double currentPositionClocks = 0.0;

    /// processLoop() runs on the audio thread, so it must not broadcast
    /// itself: ActionBroadcaster::sendActionMessage takes the listener lock
    /// and allocates a message (plus a String copy) per listener. Instead
    /// the audio thread counts the events here and triggers the async
    /// updater, whose message JUCE preallocates. handleAsyncUpdate() then
    /// sends one action message per counted event from the message thread,
    /// so a late message thread still reports every wrap - the recording
    /// handler moves its region by one loop length per message.
    std::atomic<int> pendingLoopEntries { 0 };
    std::atomic<int> pendingLoopWraps { 0 };

    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportLoop)
};

} // namespace audium
