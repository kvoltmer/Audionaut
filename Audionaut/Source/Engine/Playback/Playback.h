//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Voice.h"
#include "PlaybackDefines.h"

namespace audium
{

/**
 * @class Playback
 * @brief Manages audio playback and voice allocation for multitrack recordings.
 *
 * The `Playback` class handles the preparation, processing, and management of audio playback.
 * It provides functionality for starting and stopping voices, processing audio blocks, and
 * managing the pool of available voices.
 */
class Playback
{
public:
    /**
     * @brief Constructs a `Playback` instance.
     */
    Playback();

    /**
     * @brief Default destructor for `Playback`.
     */
    ~Playback() = default;

    /**
     * @brief Prepares the playback system for audio processing.
     * @param samplesPerBlockExpected The expected number of samples per block.
     * @param sampleRate The sample rate for audio processing.
     */
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);

    /**
     * @brief Starts a voice for the specified transport source.
     * @param source Shared pointer to the `VoiceSource` to start.
     * @return True if a voice was successfully started, false otherwise.
     */
    bool startVoice(std::shared_ptr<VoiceSource> source);

    /**
     * @brief Stops the voice associated with the specified transport source.
     * @param source Shared pointer to the `VoiceSource` to stop.
     * @return True if the voice was successfully stopped, false otherwise.
     */
    bool stopVoice(const std::shared_ptr<VoiceSource> source,
                   bool fadeOutLastBlock);

    /**
     * @brief Stops all active voices.
     */
    void stopAllVoices();

    /**
     * @brief Checks if the specified transport source is currently playing.
     * @param source Shared pointer to the `VoiceSource` to check.
     * @return True if the source is playing, false otherwise.
     */
    bool isPlaying(const std::shared_ptr<VoiceSource> source);

    /**
     * @brief Process audio playback.
     */
    template <typename ProcessContext>
    void process (const ProcessContext& context) noexcept
    {
        auto& outputBlock = context.getOutputBlock();
        auto numSamples = static_cast<int>(outputBlock.getNumSamples());
        auto numChannels = std::min(static_cast<int>(outputBlock.getNumChannels()),
                                    MAX_AUDIO_CHANNELS);
        
        // TODO: avoid reallocating in the audio thread
        processingBuffer.setSize(numChannels, numSamples, false, false, true);
        processingBuffer.clear();
        
        juce::dsp::AudioBlock<float> processingBlock(processingBuffer);
        
        for (auto i = 0; i < MAX_VOICES; ++i) {
            if (voices[i].processing.load()) {
                juce::AudioSourceChannelInfo info (&processingBuffer, 0, numSamples);
                voices[i].processAudioBlock(info);
                outputBlock.add ( processingBlock );
            }
        }
    }
        
    /**
     * @brief Gets the number of available voices.
     * @return The number of voices.
     */
    int getNumVoices() const;

private:
    /**
     * @brief Gets an available voice for playback.
     * @return Pointer to an available `Voice`, or nullptr if none are available.
     */
    Voice* getAvailableVoice();

    /**
     * @brief Finds the voice associated with the specified transport source.
     * @param source Shared pointer to the `VoiceSource` to find.
     * @return Pointer to the associated `Voice`, or nullptr if not found.
     */
    Voice* findVoice(const std::shared_ptr<VoiceSource> source);

    /**
     * @brief Gets the total number of voices.
     * @return The total number of voices.
     */
    int getNumberOfVoices() const;

    juce::AudioBuffer<float> processingBuffer; ///< Buffer for audio bus processing.

    Voice voices[MAX_VOICES]; ///< Array of voices for playback.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Playback)
};

} // namespace audium
