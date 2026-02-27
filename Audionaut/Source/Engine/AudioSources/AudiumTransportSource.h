//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <JuceHeader.h>

#include "Engine/PlayList/SampleTimer.h"
#include "Engine/AudioSources/audium_AudioTransportSource.h"

namespace audium {

#define MAX_AUDIO_FILE_CHANNELS 64

class AudioTrack;
class AudioResource;
class PlayListItem;
class TempoProvider;
/**
 * @class AudiumTransportSource
 * @brief A wrapper around `juce::AudioSource` for managing audio playback with additional features.
 *
 * This class provides functionality for scheduling playback positions, managing playback states,
 * and applying channel mappings. It wraps around `juce::AudioSource` and integrates with
 * `AudioResource` and `AudioTransportSource` for advanced audio playback control.
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
        audioTransportSource->setSource(nullptr);
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
        if (mainSource != nullptr)
            mainSource->releaseResources();
    }
    
    /**
     * @brief Schedules a position change during playback.
     * @param newPosition The new playback position in seconds.
     * @param startSample The sample position where the change should occur.
     */
    void schedulePosition (double newPosition, int startSample)
    {
        //std::cout << "schedulePosition " << newPosition << " " << startSample << std::endl;
        if (startSample == 0) {
            audioTransportSource->setPosition(newPosition);
        }
        else {
            scheduledStartSample.store(startSample);
            scheduledPosition = newPosition;
        }
    }
    
    void scheduleDuration(double duration, double sr)
    {
        durationTimer.schedule(static_cast<int>(duration * sr));
    }
    
    bool isPlaying() const noexcept
    {
        return audioTransportSource->isPlaying();
    }
    
    bool isStopped() const noexcept
    {
        return audioTransportSource->isStopped();
    }
    
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override;
    
    
    AudioResource& getAudioResource() const { return audioResource; }
    
    void applyChannelMapping(bool withChannelOffset = true);
    
    std::shared_ptr<audium::AudioTransportSource> getAudioTransportSource() const { return audioTransportSource; }
    
    /// < configure gain, fade-in, fade-out
    void configureDynamics(std::shared_ptr<PlayListItem> item,
                           std::shared_ptr<TempoProvider> tempoProvider);
    
#if CATCH2_TESTS
    int64 samplesProcessed = 0;
#endif
    
private:
    AudioResource& audioResource; ///< Reference to the associated audio resource.
    std::atomic<int> scheduledStartSample = 0; ///< The sample position for a scheduled position change.
    std::atomic<double> scheduledPosition = 0.0; ///< The scheduled playback position in seconds.
    audium::SampleTimer durationTimer; ///< Timer for managing playback duration.
    std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource; ///< Audio format reader source.
    std::shared_ptr<audium::AudioTransportSource> audioTransportSource; ///< Underlying audio transport source.
    std::unique_ptr<juce::ChannelRemappingAudioSource> channelRemapping; ///< Channel remapping audio source.
    juce::AudioSource* mainSource = nullptr; ///< Pointer to the main audio source.
    
};

} // namespace audium 
