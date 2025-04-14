//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <JuceHeader.h>

namespace audium {

class AudioResourceContainer;
class AudioTrack;
class AudiumTransportSource;
class AudioResource;
class Playback;

/**
 * @class TransportSourceContainer
 * @brief Manages a collection of transport sources for audio playback.
 *
 * This class is responsible for creating, managing, and cleaning up transport
 * sources associated with audio resources. It provides methods to prepare
 * playback, manage transport sources, and apply channel mappings.
 */
class TransportSourceContainer
{
public:
    /**
     * @brief Constructs a TransportSourceContainer instance.
     * @param playback_ Shared pointer to the Playback instance.
     */
    TransportSourceContainer(std::shared_ptr<audium::Playback> playback_) :
        playback(playback_)
    {}

    /**
     * @brief Destructor for TransportSourceContainer.
     */
    ~TransportSourceContainer() = default;

    /**
     * @brief Prepares the transport sources for playback.
     * @param samplesPerBlockExpected The expected number of samples per block.
     * @param sampleRate The sample rate of the audio playback.
     */
    void prepareToPlay (int samplesPerBlockExpected,
                        double sampleRate);

    /**
     * @brief Cleans up the transport sources and releases resources.
     */
    void cleanup();

    /**
     * @brief Creates and adds a transport source for the given audio resource.
     * @param audioResource The audio resource to associate with the transport source.
     * @param audioFormatReaderSource Shared pointer to the audio format reader source.
     * @return Shared pointer to the created AudiumTransportSource.
     */
    std::shared_ptr<AudiumTransportSource> createAndAddTransportSource(AudioResource& audioResource,
                                                                       std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource);

    /**
     * @brief Removes a transport source from the container.
     * @param audioTransportSource Shared pointer to the transport source to remove.
     * @return True if the transport source was removed, false otherwise.
     */
    bool removeTransportSource(std::shared_ptr<AudiumTransportSource> audioTransportSource);

    /**
     * @brief Retrieves all transport sources associated with a specific audio resource.
     * @param resource The audio resource to search for.
     * @return A vector of shared pointers to the matching transport sources.
     */
    std::vector<std::shared_ptr<AudiumTransportSource>> getTransportSourcesForResource(const AudioResource &resource) const;

    /**
     * @brief Retrieves the transport source at the specified index.
     * @param index The index of the transport source.
     * @return Shared pointer to the transport source at the given index.
     */
    std::shared_ptr<AudiumTransportSource> getTransportSourceAtIndex(int index) const;

    /**
     * @brief Retrieves the index of a specific transport source.
     * @param searchTransportSource Shared pointer to the transport source to search for.
     * @return The index of the transport source, or -1 if not found.
     */
    int getTransportSourceIndex(std::shared_ptr<AudiumTransportSource> searchTransportSource) const;

    /**
     * @brief Applies channel mapping to the transport sources.
     */
    void applyChannelMapping();

private:
    std::shared_ptr<audium::Playback> playback; ///< Shared pointer to the Playback instance.
    std::vector<std::shared_ptr<AudiumTransportSource>> audioTransportSources; ///< Collection of transport sources.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportSourceContainer)
};

} // namespace audium

