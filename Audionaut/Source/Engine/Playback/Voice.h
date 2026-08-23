//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

namespace audium
{

class VoiceSource;

/**
 * @class Voice
 * @brief Represents a voice that processes audio blocks and manages playback.
 *
 * The `Voice` class is responsible for handling audio processing and controlling
 * the playback of an associated `VoiceSource`. It provides methods to
 * start and stop playback, as well as to process audio blocks in real-time.
 */
class Voice
{
public:
    /**
     * @brief Constructs a `Voice` instance.
     */
    Voice() = default;

    /**
     * @brief Default destructor for `Voice`.
     */
    ~Voice() = default;

    /**
     * @brief Processes an audio block.
     * @param bufferToFill The `juce::AudioSourceChannelInfo` containing the buffer to process.
     */
    void processAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill);

    /**
     * @brief Starts playback using the specified transport source.
     * @param voiceSource Shared pointer to the `VoiceSource` to use for playback.
     */
    void start(std::shared_ptr<VoiceSource> voiceSource);

    /**
     * @brief Stops playback.
     */
    void stop(bool fadeOutLastBlock);

    /**
     * @brief Checks if the voice is currently processing audio.
     * @return True if the voice is processing, false otherwise.
     */
    std::atomic<bool> processing;

    /**
     * @brief Gets the associated transport source.
     * @return A shared pointer to the `VoiceSource`.
     */
    const std::shared_ptr<VoiceSource> getVoiceSource() const { return voiceSource; }

private:
    std::shared_ptr<VoiceSource> voiceSource; ///< Shared pointer to the transport source.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Voice)
};

} // namespace audium
