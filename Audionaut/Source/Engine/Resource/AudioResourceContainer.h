//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <vector>
#include <memory>
#include <JuceHeader.h>

#include "Engine/Resource/AudioResource.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/ResourceGroup.h"
#include "Engine/Provider/TempoProvider.h"

namespace audium {

class TransportSourceContainer;
class AudiumEngine;

/**
 * @class AudioResourceContainer
 * @brief Manages audio resources, including their creation, retrieval, and removal.
 *
 * The `AudioResourceContainer` class provides functionality to manage audio resources
 * in the Audionaut application. It handles operations such as adding, removing, and
 * finding audio resources, as well as managing their associated tracks and groups.
 */
class AudioResourceContainer : public juce::ActionBroadcaster
{
public:
    /**
     * @brief Constructs an `AudioResourceContainer` with the specified dependencies.
     * @param audioDeviceManager_ A shared pointer to the `AudioDeviceManager`.
     * @param formatManager_ A shared pointer to the `AudioFormatManager`.
     * @param audioThumbnailCache_ A shared pointer to the `AudioThumbnailCache`.
     * @param tempoProvider_ A shared pointer to the `TempoProvider`.
     * @param transportSourceContainer_ A shared pointer to the `TransportSourceContainer`.
     */
    AudioResourceContainer(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager_,
                           std::shared_ptr<juce::AudioFormatManager> formatManager_,
                           std::shared_ptr<juce::AudioThumbnailCache> audioThumbnailCache_,
                           std::shared_ptr<TempoProvider> tempoProvider_,
                           std::shared_ptr<TransportSourceContainer> transportSourceContainer_) :
        audioDeviceManager(audioDeviceManager_),
        formatManager(formatManager_),
        audioThumbnailCache(audioThumbnailCache_),
        tempoProvider(tempoProvider_),
        transportSourceContainer(transportSourceContainer_)
    {
        formatManager->registerBasicFormats();
        thread.startThread();
    }
    

    /**
     * @brief Destructor for `AudioResourceContainer`.
     */
    ~AudioResourceContainer();

    /**
     * @brief Creates a temporary project directory.
     * @param reset Whether to reset the directory if it already exists.
     * @return True if the directory was successfully created, false otherwise.
     */
    static bool createTemporaryProjectDirectory(bool reset);

    /**
     * @brief Retrieves the directory for audio files within the project.
     * @param projectRoot The root directory of the project.
     * @return The audio file directory as a `juce::File`.
     */
    static const juce::File getAudioFileDirectory(const juce::File projectRoot);

    /**
     * @brief Retrieves the default audio file directory.
     * @return The audio file directory as a `juce::File`.
     */
    static const juce::File getAudioFileDirectory();

    
    bool isAudioFileCurrentlyLoaded(const juce::File audioFile) const;
    
    /**
     * @brief Copies or moves audio files between directories.
     * @param sourceDirectory The source directory.
     * @param destinationDirectory The destination directory.
     */
    bool copyOrMoveAudioFiles(const juce::File sourceDirectory, const juce::File destinationDirectory);

    /**
     * @brief Changes the paths of audio files to a new directory.
     * @param newPath The new directory path.
     */
    void changeAudioFilePaths(const juce::File newPath);

    /**
     * @brief Copies an audio file to the audio file directory if needed.
     * @param url The URL of the audio file.
     * @return The URL of the copied file or the original URL if no copy was needed.
     */
    static const juce::URL copyToAudioFileDirectoryIfNeeded(juce::URL url);

    /**
     * @brief Finds an audio resource with the specified URL.
     * @param url The URL of the audio resource.
     * @return A shared pointer to the `AudioResource`, or nullptr if not found.
     */
    std::shared_ptr<AudioResource> findResourceWithUrl(juce::URL url) const;

    /**
     * @brief Retrieves an `AudioFormatReader` for the specified URL.
     * @param url The URL of the audio file. This method may modify the URL.
     * @return A shared pointer to the `AudioFormatReader`.
     */
    std::shared_ptr<juce::AudioFormatReader> getAudioFormatReaderForUrl(juce::URL &url);

    /**
     * @brief Adds a new audio resource.
     * @param url The URL of the audio file.
     * @param audioFormatReader A shared pointer to the `AudioFormatReader`.
     * @param track A shared pointer to the associated `AudioTrack`.
     * @param resourceGroup A shared pointer to the associated `ResourceGroup`.
     * @param destChannel The destination channel index (default: -1).
     * @param sourceChannel The source channel index (default: -1).
     * @return A shared pointer to the created `AudioResource`.
     */
    std::shared_ptr<AudioResource> addAudioResource(juce::URL url,
                                                    std::shared_ptr<juce::AudioFormatReader> audioFormatReader,
                                                    std::shared_ptr<AudioTrack> track,
                                                    std::shared_ptr<ResourceGroup> resourceGroup,
                                                    int destChannel = -1,
                                                    int sourceChannel = -1);

    /**
     * @brief Creates a transport source for the specified audio resource.
     * @param audioResource A shared pointer to the `AudioResource`.
     * @return A shared pointer to the created `AudiumTransportSource`.
     */
    std::shared_ptr<AudiumTransportSource> createTransportSourceForAudioResource(std::shared_ptr<AudioResource> audioResource);

    /**
     * @brief Removes the specified audio resource.
     * @param resource A shared pointer to the `AudioResource` to remove.
     */
    void removeAudioResource(std::shared_ptr<AudioResource> resource);

    /**
     * @brief Removes all audio resources associated with the specified track.
     * @param track A pointer to the `AudioTrack`.
     */
    void removeAudioResourcesForTrack(AudioTrack *track);

    /**
     * @brief Retrieves the number of audio resources.
     * @return The number of audio resources.
     */
    int getNumAudioResources() const;

    /**
     * @brief Retrieves audio resources at the specified absolute position.
     * @param positionInSeconds The position in seconds.
     * @return A vector of shared pointers to `AudioResource` objects.
     */
    std::vector<std::shared_ptr<AudioResource>> resourcesAtAbsolutePosition(double positionInSeconds) const;

    /**
     * @brief Cleans up the container by removing unused resources.
     */
    void cleanup();

    /**
     * @brief Retrieves audio resources associated with the specified track.
     * @param track A pointer to the `AudioTrack`.
     * @return A vector of shared pointers to `AudioResource` objects.
     */
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesForTrack(AudioTrack *track) const;

    /**
     * @brief Retrieves audio resources within the specified subgroup.
     * @param resourceGroup A pointer to the `ResourceGroup`.
     * @return A vector of shared pointers to `AudioResource` objects.
     */
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesForResourceGroup(const ResourceGroup *resourceGroup) const;

    /**
     * @brief Retrieves audio resources for a track within a specified range.
     * @param track A pointer to the `AudioTrack`.
     * @param rangeInSeconds The range in seconds.
     * @return A vector of shared pointers to `AudioResource` objects.
     */
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesForTrackAtAbsoluteRange(AudioTrack *track, juce::Range<double> rangeInSeconds) const;

    /**
     * @brief Retrieves audio resources for the specified channel.
     * @param channel A pointer to the `AudioChannel`.
     * @return A vector of shared pointers to `AudioResource` objects.
     */
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesForChannel(const AudioChannel *channel) const;

    /**
     * @brief Retrieves the audio track associated with the specified resource.
     * @param resource A shared pointer to the `AudioResource`.
     * @return A shared pointer to the `AudioTrack`.
     */
    std::shared_ptr<AudioTrack> getAudioTrackForResource(std::shared_ptr<AudioResource> resource) const;

    /**
     * @brief Retrieves all audio tracks in the container.
     * @return A vector of shared pointers to `AudioTrack` objects.
     */
    std::vector<std::shared_ptr<AudioTrack>> getAudioTracks() const;

    /**
     * @brief Handles the deletion of a channel from a track.
     * @param audioTrack A pointer to the `AudioTrack`.
     * @param channel A pointer to the `AudioChannel`.
     */
    void onDeleteChannel(AudioTrack* audioTrack, AudioChannel* channel);
    
    /**
     * @brief Retrieves the `AudioFormatManager`.
     * @return A shared pointer to the `AudioFormatManager`.
     */
    std::shared_ptr<juce::AudioFormatManager> getAudioFormatManager() const { return formatManager; }
    
    /**
     * @brief Retrieves the `AudioThumbnailCache`.
     * @return A shared pointer to the `AudioThumbnailCache`.
     */
    std::shared_ptr<juce::AudioThumbnailCache> getAudioThumbnailCache() const { return audioThumbnailCache; }
    
    /**
     * @brief Retrieves the `TempoProvider`.
     * @return A shared pointer to the `TempoProvider`.
     */
    std::shared_ptr<TempoProvider> getTempoProvider() const { return tempoProvider; }
    
    /**
     * @brief Retrieves the `AudioDeviceManager`.
     * @return A shared pointer to the `AudioDeviceManager`.
     */
    std::shared_ptr<juce::AudioDeviceManager> getAudioDeviceManager() const { return audioDeviceManager; }
    
    /**
     * @typedef tAudioTrackPair
     * @brief A type alias for a pair of `AudioTrack` and `AudioResource`.
     */
    typedef std::pair<std::shared_ptr<AudioTrack>, std::shared_ptr<AudioResource>> tAudioTrackPair;
    
    /**
     * @brief Retrieves the read-ahead thread.
     * @return A pointer to the `TimeSliceThread`.
     */
    juce::TimeSliceThread *getReadAheadThread() { return &thread; }
    
private:
    /// List of pairs of audio tracks and resources.
    std::list<tAudioTrackPair> audioResources;

    /// Shared pointer to the `AudioDeviceManager`.
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;

    /// Shared pointer to the `AudioFormatManager`.
    std::shared_ptr<juce::AudioFormatManager> formatManager;

    /// Shared pointer to the `AudioThumbnailCache`.
    std::shared_ptr<juce::AudioThumbnailCache> audioThumbnailCache;

    /// Shared pointer to the `TempoProvider`.
    std::shared_ptr<TempoProvider> tempoProvider;

    /// Shared pointer to the `TransportSourceContainer`.
    std::shared_ptr<TransportSourceContainer> transportSourceContainer;

    /// Thread for read-ahead operations.
    juce::TimeSliceThread thread { "read ahead thread" };

    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResourceContainer)
};

} // namespace audium
