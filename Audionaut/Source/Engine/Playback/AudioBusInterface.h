//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/Playback/AudioBusRenderer.h"
#include "Engine/Playback/RenderThreadPool.h"
#include "Engine/Core/LockFreeCommander.h"
#include "Engine/Channel/AudioChannelData.h"
#include <optional>

namespace audium
{
    class AudioThumbnail;

/**
 * @brief The message-thread face of the audio bus: channel data and levels,
 *        commands to the renderer, and the render thread pool's lifecycle.
 */
class AudioBusInterface
{
    
public:
    /**
    * @brief Constructs an `AudioBusInterface` instance.
    * @param lockFreeCommander_ Shared pointer to the lock-free commander for thread-safe operations.
    * @param audioBusRenderer_ Shared pointer to the audio bus renderer for audio processing.
    * @param renderThreadPool_ The pool the renderer runs its track jobs on; this
    *        interface starts and stops it with the audio device.
    */
    AudioBusInterface(std::shared_ptr<audium::LockFreeCommander> lockFreeCommander_,
                  std::shared_ptr<audium::AudioBusRenderer<float>> audioBusRenderer_,
                  std::shared_ptr<RenderThreadPool> renderThreadPool_) :
        lockFreeCommander(lockFreeCommander_),
        audioBusRenderer(audioBusRenderer_),
        renderThreadPool(std::move(renderThreadPool_))
    {
    }

    /**
    * @brief Default destructor for `AudioBusInterface`.
    */
    ~AudioBusInterface() = default;

    /**
    * @brief Prepares the audio bus for playback.
    * @param samplesPerBlockExpected The expected number of samples per block.
    * @param sampleRate The sample rate for audio processing.
    */
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate);

    template <typename ProcessContext>
    void process (const ProcessContext& context) noexcept
    {
        lockFreeCommander->invoke();
        audioBusRenderer->process(context);
    }

    
    void invokeCommands () noexcept
    {
        lockFreeCommander->invoke();
    }
    
    /**
    * @brief Sets the number of audio bus channels.
    * @param numChannels The number of channels to set.
    */
    void setNumAudioBusChannels(int numChannels);

    /**
    * @brief The bus channel count the last block rendered with.
    */
    int getNumAudioBusChannels() const noexcept;

    /**
    * @brief Sets the channel data for a specific channel.
    */
    void setChannelData(const int channelNumber, const AudioChannelData data);

    /**
     * @brief Whether channel commands are drained inline instead of by a
     *        render callback.
     *
     * Defaults to `HeadlessMode`, which is process-wide and therefore wrong
     * for a scratch engine running inside the GUI: the scratch engine has no
     * device and must pump its own fifo, while the live engine's device is
     * draining as usual and must not. Set it explicitly on such an engine.
     */
    void setPumpsCommandsSynchronously(bool shouldPump) noexcept { pumpsCommandsSynchronously = shouldPump; }

    /**
     * @brief The worker threads that share the live callback's track jobs.
     *
     * Started by the device callback's about-to-start (LinkAudioDevice) for
     * the device it runs with, and stopped when the device stops. Offline
     * renders never use it.
     */
    RenderThreadPool& getRenderThreadPool() noexcept { return *renderThreadPool; }

    /**
     * @brief How many worker threads the pool starts with the next device
     *        start: -1 = the default for this machine (RenderThreadPool::defaultNumWorkers),
     *        0 = none (the serial path).
     */
    void setNumRenderThreads(int numWorkers) noexcept { numRenderThreads.store(numWorkers); }
    int getNumRenderThreads() const noexcept { return numRenderThreads.load(); }

    /** The track jobs the last block was split into (tests). */
    int getNumTrackJobs() const noexcept { return audioBusRenderer->getNumTrackJobs(); }

    /** Per-job, per-thread shares of the last block for the Processing Load window; see AudioBusRenderer. */
    void setTrackJobSharesEnabled(bool enabled) noexcept { audioBusRenderer->setTrackJobSharesEnabled(enabled); }
    int readTrackJobShares(TrackJobShare* out, int capacity) const noexcept { return audioBusRenderer->readTrackJobShares(out, capacity); }

   #if JUCE_DEBUG
    /** Debug builds: extra busy time for a track's job, see AudioBusRenderer::setSimulatedTrackLoad. */
    void setSimulatedTrackLoad(int trackId, float share, bool bursty) noexcept { audioBusRenderer->setSimulatedTrackLoad(trackId, share, bursty); }
   #endif

    /** The worker count the setting resolves to. */
    int resolveNumRenderThreads() const noexcept
    {
        const auto configured = numRenderThreads.load();
        return configured < 0 ? RenderThreadPool::defaultNumWorkers() : configured;
    }
    
    const AudioChannelData getChannelData(const int channelNumber) const;

    
    void setRecordEnabled(const int channelNumber,
                          bool bEnabled);
        
    std::shared_ptr<audium::AudioThumbnail> getRecordingThumbnail(int channelNumber) const;
    
    void record(bool start, const int channelNumber = -1, const double positionClocks = -1.0);

    /** The number of the take being (or last) recorded; see Recording::getCurrentTake. */
    int getCurrentTake() const;

    const juce::File getRecordedAudioFile(int channelNumber);
    
    const double getRecordedLength(int channelNumber) const;
    
    const double getRecordingStartPosition(int channelNumber) const;
    
    bool isRecording(int channelNumber) const;
    
    bool anyChannelRecording() const;
    
    /**
    * @brief Sets the master gain for the audio bus.
    * @param newGain The new master gain value.
    */
    void setMasterGain(const float newGain);
    
    void resetGains();

    void stopAllVoices();

    /**
    * @brief Enables/disables the offline bounce mapping (identity bus -> output copy,
    * bypassing output routing). Call only while the live device callback is bypassed.
    */
    void setStemExport(bool bStemExport);

    /**
    * @brief Gets the current level of a specific channel.
    * @param channelNumber The channel index.
    * @return The current level of the channel.
    */
    const float getChannelLevel(const int channelNumber) const;

    /**
    * @brief Gets the current level of a recording enabled channel.
    * @param channelNumber The channel index.
    * @return The current level of the channel.
    */
    const float getRecordingLevel(const int channelNumber) const;
    
    /**
    * @brief Gets the current master level for a specific channel.
    * @param channelNumber The channel index.
    * @return The current master level of the channel.
    */
    const float getMasterLevel(const int channelNumber) const;
    

 private:
     /// Unset = follow HeadlessMode (the process-wide default).
     std::optional<bool> pumpsCommandsSynchronously;

     std::shared_ptr<audium::LockFreeCommander> lockFreeCommander; ///< Thread-safe command manager.
     std::shared_ptr<audium::AudioBusRenderer<float>> audioBusRenderer; ///< Renderer for audio bus processing.
     std::shared_ptr<RenderThreadPool> renderThreadPool;
     std::atomic<int> numRenderThreads { -1 };

     JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioBusInterface)
 };

 } // namespace audium
