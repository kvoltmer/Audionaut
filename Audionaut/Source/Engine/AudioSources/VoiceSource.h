//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <JuceHeader.h>

#include "Engine/PlayList/SampleTimer.h"
#include "Engine/AudioSources/ClipTransportSource.h"
#include "Engine/Core/DspClipData.h"

namespace audium {

class AudioResource;
class PlayListItem;
struct ClipFadeSpec;
/**
 * @class VoiceSource
 * @brief A wrapper around `juce::AudioSource` for managing audio playback with additional features.
 *
 * This class provides functionality for scheduling playback positions, managing playback states,
 * and applying channel mappings. It wraps around `juce::AudioSource` and integrates with
 * `AudioResource` and `ClipTransportSource` for advanced audio playback control.
 */
class VoiceSource : public juce::AudioSource
{
public:
    /**
     * @brief Constructs a VoiceSource instance.
     * @param audioResource Reference to the associated audio resource.
     * @param audioFormatReaderSource Shared pointer to the audio format reader source.
     */
    VoiceSource(AudioResource& audioResource,
                          std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource);
    
    /**
     * @brief Destructor for VoiceSource.
     *
     * Ensures that the clip voice source is properly released.
     */
    ~VoiceSource() override
    {
        clipTransportSource->setSource(nullptr);
    }
    
    /**
     * @brief Prepares the audio source for playback.
     * @param samplesPerBlockExpected The expected number of samples per block.
     * @param sampleRate The sample rate of the audio playback.
     */
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    
    /**
     * @brief Releases resources used by the audio source.
     */
    void releaseResources() override
    {
        clipTransportSource->releaseResources();
    }
    
    /**
     * @brief Schedules a position change during playback.
     * @param newPosition The new playback position in seconds.
     * @param startSample The sample position where the change should occur.
     */
    void schedulePosition (double newPosition, int startSample)
    {
        if (startSample == 0) {
            clipTransportSource->setPosition(newPosition);
        }
        else {
            scheduledStartSample.store(startSample);
            scheduledPosition = newPosition;
            reScheduled = isPlaying();
        }
    }
    
    /// Ends the voice after @p numSamples output samples.
    void scheduleDurationSamples(int numSamples)
    {
        durationTimer.schedule(numSamples);
    }
    
    bool isPlaying() const noexcept
    {
        return clipTransportSource->isPlaying();
    }
    
    bool isStopped() const noexcept
    {
        return clipTransportSource->isStopped();
    }

    void start()
    {
        clipTransportSource->start();
    }

    void stop(bool fadeOutLastBlock)
    {
        clipTransportSource->stop(fadeOutLastBlock);
    }

    /// The clip's playback speed - see ClipTransportSource::setSpeedRatio.
    void setSpeedRatio(double newSpeedRatio)
    {
        clipTransportSource->setSpeedRatio(newSpeedRatio);
    }

    double getSpeedRatio() const noexcept
    {
        return clipTransportSource->getSpeedRatio();
    }

    /// The speed changed mid-clip (a tempo-locked clip following a tempo
    /// change): the scheduled end moves by @p factor = old / new speed.
    void rescaleRemainingDuration(double factor)
    {
        durationTimer.rescale(factor);
    }

    /// Varispeed vs pitch-preserving - see ClipTransportSource::setStretchMode.
    void setStretchMode(StretchMode newMode)
    {
        clipTransportSource->setStretchMode(newMode);
    }

    /// Message thread: the clip entered Stretch mode - see
    /// ClipTransportSource::allocateStretcher.
    void allocateStretcher()
    {
        clipTransportSource->allocateStretcher();
    }

    /** One slice of the standby prime for a known upcoming jump (the loop
        wrap) of a Stretch-mode clip; call once per block over the blocks
        before it. See ClipTransportSource::primeStandby. Real-time safe. */
    void primeStandby(double filePositionSeconds, double ratio, int blocksLeft)
    {
        clipTransportSource->primeStandby(filePositionSeconds, ratio, blocksLeft);
    }

    const ClipTransportSource& getClipTransportSource() const noexcept { return *clipTransportSource; }

    /** The clip data this voice was last scheduled from (audio thread) - a
        fresh clip snapshot compares against it, so a voice only restarts
        when its own clip changed, not when the generation did. */
    const DspClipData& getScheduledClipData() const noexcept { return scheduledClipData; }
    void setScheduledClipData(const DspClipData& data) noexcept { scheduledClipData = data; }

    /** A snapshot change moved this playing Stretch-mode clip: the live lane
        keeps rendering its old alignment up to  transportSeconds while the
        standby lane primes for that position, and the restart there only
        swaps lanes (see PlayListScheduler::onClipSnapshotChanged). Audio
        thread only; cleared by every schedule. */
    void deferRestart(double transportSeconds) noexcept { pendingRestartAt = transportSeconds; }
    double getPendingRestartAt() const noexcept { return pendingRestartAt; }
    void clearPendingRestart() noexcept { pendingRestartAt = -1.0; }

    void setGain(float gain)
    {
        clipTransportSource->setGain(gain);
    }

    void resetClipGain()
    {
        clipTransportSource->resetClipGain();
    }

    /** Configures both clip fades on the inner transport (see the free
        configureClipFades in ClipFadeSpec.h). Real-time safe. */
    void configureClipFades(const ClipFadeSpec& spec, double filePositionSeconds, bool reset);

    /// Renders the voice's one channel into channel 0 of @p info; where it
    /// belongs is getDestinationChannel(). A one-channel buffer is enough.
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override;

    /// Whether the voice reads through a read-ahead buffer and has a standby
    /// lane for the scheduler to pre-seek to known jumps (either stretch mode).
    bool hasBufferedStandby() const noexcept { return clipTransportSource->hasBufferedStandby(); }

    AudioResource& getAudioResource() const { return audioResource; }

    void applyChannelMapping(bool withChannelOffset = true);

    /// The output channel the voice's rendered channel belongs to (set by applyChannelMapping).
    int getDestinationChannel() const noexcept { return destinationChannel.load(); }

    /// < configure gain, fade-in, fade-out
    void configureDynamics(std::shared_ptr<PlayListItem> item);
    
private:
    /// Gives a read-ahead buffer that cannot serve @p info yet a moment to catch up.
    void waitForReadAhead (const juce::AudioSourceChannelInfo& info);

    /// Renders the transport into the voice's own file-wide buffer and copies
    /// the mapped source channel into channel 0 of @p info.
    void renderChannel (const juce::AudioSourceChannelInfo& info);

    AudioResource& audioResource; ///< Reference to the associated audio resource.
    std::shared_ptr<juce::AudioFormatReader> audioFormatReader; ///< The reader the voice plays from: the resource's memory-mapped one, co-owned as a retired voice may still read it after the resource is gone, or for a compressed file a decoder of the voice's own.
    std::shared_ptr<juce::AudioFormatReader> standbyFormatReader; ///< The standby cursor's own decoder (compressed files only).
    std::shared_ptr<juce::TimeSliceThread> readAheadThread; ///< Co-owns that thread too (buffered readers only): the read-ahead buffers unregister from it when the transport below goes, and a retired voice can outlive the resource container that started it.
    std::atomic<int> scheduledStartSample   = 0; ///< The sample position for a scheduled position change.
    std::atomic<double> scheduledPosition   = 0.0; ///< The scheduled playback position in seconds.
    std::atomic<bool> reScheduled           = false; /// Helper to indicate if position is re-scheduled (loop)
    std::atomic<int> sourceChannel          = 0; ///< File channel the voice plays (channel mapping, read on the audio thread).
    std::atomic<int> destinationChannel     = 0; ///< Output channel of the channel mapping, read by Playback on the audio thread.
    audium::SampleTimer durationTimer; ///< Timer for managing playback duration.
    DspClipData scheduledClipData; ///< What scheduleClip last applied (audio thread).
    double pendingRestartAt = -1.0; ///< Timeline seconds of a deferred restart, < 0 when none (audio thread).
    std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource; ///< Audio format reader source.
    std::shared_ptr<juce::AudioFormatReaderSource> standbyReaderSource; ///< Second cursor for the transport's standby lane: on the same memory-mapped reader, or on standbyFormatReader.
    std::shared_ptr<audium::ClipTransportSource> clipTransportSource; ///< Underlying clip voice source.
    juce::AudioBuffer<float> renderBuffer; ///< The transport's output, all file channels; sized in prepareToPlay.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceSource)
};

} // namespace audium 
