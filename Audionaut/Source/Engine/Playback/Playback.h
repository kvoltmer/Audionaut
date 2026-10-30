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
    bool startVoice(VoiceSource* source);

    /**
     * @brief Stops the voice associated with the specified transport source.
     * @param source Shared pointer to the `VoiceSource` to stop.
     * @return True if a playing voice was stopped, false if the source was
     *         not playing (the scheduler asks for every idle clip, every
     *         block - that case must stay cheap).
     */
    bool stopVoice(VoiceSource* source,
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
    bool isPlaying(VoiceSource* source);

    /** True while any voice still holds the source. Message-thread check
        before a removed source may be destroyed (VoiceSourceContainer). */
    bool isVoiceSourceActive(const VoiceSource* source) const noexcept;

    /**
     * @brief Renders one voice's block into `output` (one channel, cleared first).
     *
     * Audio thread, or a render worker that owns the voice's track: a voice
     * is rendered by one thread per block, and the caller adds the result
     * to the voice's bus channel. Voices are independent of each other, so
     * different voices may render concurrently.
     *
     * @return The bus channel the rendered audio belongs to, -1 if the voice is
     *         idle or rendered nothing.
     */
    int renderVoice (int voiceIndex, float* output, int numSamples) noexcept
    {
        if (voiceIndex < 0 || voiceIndex >= MAX_VOICES || ! voices[voiceIndex].processing.load())
            return -1;

        // a buffer view over the caller's memory: no allocation
        float* channels[1] = { output };
        juce::AudioBuffer<float> view (channels, 1, numSamples);
        juce::AudioSourceChannelInfo info (&view, 0, numSamples);
        return voices[voiceIndex].processAudioBlock (info);
    }

    /// The bus channel an active voice renders into, -1 for an idle voice (audio thread).
    int getActiveVoiceChannel (int voiceIndex) const noexcept;

    /// Audio thread: Voice::stopNow for an active voice the bus cannot render.
    void stopVoiceNow (int voiceIndex) noexcept;

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
    Voice* findVoice(const VoiceSource* source);

    Voice voices[MAX_VOICES]; ///< Array of voices for playback.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Playback)
};

} // namespace audium
