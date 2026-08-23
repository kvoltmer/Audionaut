//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <JuceHeader.h>

namespace audium {

class VoiceSource;
class AudioResource;
class Playback;

/**
 * @class VoiceSourceContainer
 * @brief Manages a collection of voice sources for audio playback.
 *
 * This class is responsible for creating, managing, and cleaning up transport
 * sources associated with audio resources. It provides methods to prepare
 * playback, manage voice sources, and apply channel mappings.
 */
class VoiceSourceContainer
{
public:
    /**
     * @brief Constructs a VoiceSourceContainer instance.
     * @param playback_ Shared pointer to the Playback instance.
     */
    explicit VoiceSourceContainer(std::shared_ptr<Playback> playback_) :
        playback(std::move(playback_))
    {}

    /**
     * @brief Destructor for VoiceSourceContainer.
     */
    ~VoiceSourceContainer() = default;

    /**
     * @brief Prepares the voice sources for playback.
     * @param samplesPerBlockExpected The expected number of samples per block.
     * @param sampleRate The sample rate of the audio playback.
     */
    void prepareToPlay (int samplesPerBlockExpected,
                        double sampleRate);

    /**
     * @brief Cleans up the voice sources and releases resources.
     */
    void cleanup();

    /**
     * @brief Creates and adds a voice source for the given audio resource.
     * @param audioResource The audio resource to associate with the voice source.
     * @param audioFormatReaderSource Shared pointer to the audio format reader source.
     * @return Shared pointer to the created VoiceSource.
     */
    std::shared_ptr<VoiceSource> createAndAddVoiceSource(AudioResource& audioResource,
                                                                       std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource);

    /**
     * @brief Removes a voice source from the container.
     * @param voiceSource Shared pointer to the voice source to remove.
     * @return True if the voice source was removed, false otherwise.
     */
    bool removeVoiceSource(std::shared_ptr<VoiceSource> voiceSource);

    /**
     * @brief Retrieves all voice sources associated with a specific audio resource.
     * @param resource The audio resource to search for.
     * @return A vector of shared pointers to the matching voice sources.
     */
    std::vector<std::shared_ptr<VoiceSource>> getVoiceSourcesForResource(const AudioResource &resource) const;

    /**
     * @brief Retrieves the voice source at the specified index.
     * @param index The index of the voice source.
     * @return Shared pointer to the voice source at the given index.
     */
    std::shared_ptr<VoiceSource> getVoiceSourceAtIndex(int index) const;

    /**
     * @brief Retrieves the index of a specific voice source.
     * @param searchVoiceSource Shared pointer to the voice source to search for.
     * @return The index of the voice source, or -1 if not found.
     */
    int getVoiceSourceIndex(std::shared_ptr<VoiceSource> searchVoiceSource) const;

    /**
     * @brief Applies channel mapping to the voice sources.
     */
    void applyChannelMapping();

private:
    std::shared_ptr<Playback> playback; ///< Shared pointer to the Playback instance.
    std::vector<std::shared_ptr<VoiceSource>> voiceSources; ///< Collection of voice sources.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceSourceContainer)
};

} // namespace audium

