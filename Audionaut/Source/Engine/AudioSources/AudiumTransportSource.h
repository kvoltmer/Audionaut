//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <JuceHeader.h>

#include "Engine/PlayList/SampleTimer.h"
#include "Engine/AudioSources/ClipTransportSource.h"

namespace audium {

class AudioResource;
class PlayListItem;
struct ClipFadeSpec;
/**
 * @class AudiumTransportSource
 * @brief A wrapper around `juce::AudioSource` for managing audio playback with additional features.
 *
 * This class provides functionality for scheduling playback positions, managing playback states,
 * and applying channel mappings. It wraps around `juce::AudioSource` and integrates with
 * `AudioResource` and `ClipTransportSource` for advanced audio playback control.
 */
class AudiumTransportSource : public juce::AudioSource
{
public:
    /**
     * @brief Constructs an AudiumTransportSource instance.
     * @param audioResource Reference to the associated audio resource.
     * @param audioFormatReaderSource Shared pointer to the audio format reader source.
     */
    AudiumTransportSource(AudioResource& audioResource,
                          std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource);
    
    /**
     * @brief Destructor for AudiumTransportSource.
     *
     * Ensures that the audio transport source is properly released.
     */
    ~AudiumTransportSource() override
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
        channelRemapping->releaseResources();
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
    
    void scheduleDuration(double duration, double sr)
    {
        durationTimer.schedule(static_cast<int>(duration * sr));
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

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override;


    AudioResource& getAudioResource() const { return audioResource; }

    void applyChannelMapping(bool withChannelOffset = true);

    /// < configure gain, fade-in, fade-out
    void configureDynamics(std::shared_ptr<PlayListItem> item);
    
private:
    AudioResource& audioResource; ///< Reference to the associated audio resource.
    std::atomic<int> scheduledStartSample   = 0; ///< The sample position for a scheduled position change.
    std::atomic<double> scheduledPosition   = 0.0; ///< The scheduled playback position in seconds.
    std::atomic<bool> reScheduled           = false; /// Helper to indicate if position is re-scheduled (loop)
    audium::SampleTimer durationTimer; ///< Timer for managing playback duration.
    std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource; ///< Audio format reader source.
    std::shared_ptr<audium::ClipTransportSource> clipTransportSource; ///< Underlying audio transport source.
    std::unique_ptr<juce::ChannelRemappingAudioSource> channelRemapping; ///< Channel remapping source; the outermost source in the chain.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumTransportSource)
};

} // namespace audium 
