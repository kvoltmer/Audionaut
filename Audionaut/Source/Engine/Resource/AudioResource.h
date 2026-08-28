//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "Engine/TimeContext.h"
#include "Engine/Streamable.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Resource/AudioResourceContainer.h"


namespace audium {

class AudioPlayer;
class VoiceSource;
class ResourceGroup;
class AudioRegion;
class AudioChannel;
class ChannelMapping;

/**
 * @class AudioResource
 * @brief Represents an audio resource in the Audionaut application.
 *
 * The `AudioResource` class provides functionality to manage audio files, including
 * their metadata, playback, and association with tracks and resource groups.
 */
class AudioResource : public audium::Streamable
{
    
public:
    /**
     * @brief Constructs an `AudioResource` with the specified parameters.
     * @param audioResourceContainer The container managing this resource.
     * @param audioTrack A shared pointer to the associated `AudioTrack`.
     * @param resourceGroup A shared pointer to the associated `ResourceGroup`.
     * @param url The URL of the audio file.
     * @param reader A shared pointer to the `AudioFormatReader` for the audio file.
     * @param destChannel The destination channel index.
     * @param sourceChannel The source channel index.
     */
    AudioResource(AudioResourceContainer& audioResourceContainer,
                  std::shared_ptr<AudioTrack> audioTrack,
                  std::shared_ptr<ResourceGroup> resourceGroup,
                  juce::URL url,
                  std::shared_ptr<juce::AudioFormatReader> reader,
                  int destChannel,
                  int sourceChannel
                  );
    
    /**
     * @brief Destructor for `AudioResource`.
     */
    virtual ~AudioResource() override;
    
    /**
     * @brief Creates a new transport source for playback.
     * @param audioFormatReaderSource A shared pointer to the `AudioFormatReaderSource`.
     * @return A shared pointer to the created `VoiceSource`.
     */
    std::shared_ptr<VoiceSource> createNewTransportSource(std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource);
    
    /**
     * @brief Retrieves the file name without its extension.
     * @return The file name as a `juce::String`.
     */
    const juce::String getFileNameWithoutExtension() const;
    
    /**
     * @brief Retrieves the full path name of the file.
     * @return The full path name as a `juce::String`.
     */
    const juce::String getFullPathName() const;
    
    /**
     * @brief Retrieves the URL of the audio file.
     * @return The URL as a `juce::URL`.
     */
    const juce::URL getUrl() const { return url; }
    
    /**
     * @brief Sets a new URL for the audio file.
     * @param newUrl The new URL to set.
     */
    void setUrl(const juce::URL newUrl) { url = newUrl; }
    
    /**
     * @brief Retrieves the URL as a string.
     * @return The URL as a `juce::String`.
     */
    const juce::String getUrlAsString() const;
    
    /**
     * @brief Retrieves the relative path of the file.
     * @param directoryToBeRelativeTo The directory to calculate the relative path from.
     * @return The relative path as a `juce::String`.
     */
    const juce::String getRelativePath(const juce::File &directoryToBeRelativeTo) const;
    
    /**
     * @brief Retrieves the container managing this resource.
     * @return A reference to the `AudioResourceContainer`.
     */
    AudioResourceContainer& getContainer() const { return owner; }
    
    /**
     * @brief Retrieves the sample rate of the audio file.
     * @return The sample rate as a `double`.
     */
    double getSampleRate() const;
    
    /**
     * @brief Retrieves the number of channels in the audio file.
     * @return The number of channels as an unsigned integer.
     */
    unsigned int getNumAudioFileChannels() const;
    
    /**
     * @brief Retrieves the bit depth of the audio file.
     * @return The bit depth as an unsigned integer.
     */
    unsigned int getBitDepth() const;
    
    /**
     * @brief Retrieves the length of the audio file in the specified time context.
     * @param context The time context to use.
     * @return The length of the file as a `double`.
     */
    double getFileLength(audium::TimeContextType context) const;
    
    /**
     * @brief The output channel number.
     */
    int getOutputChannelNumber() const;

    /**
     * @brief Load the recorded audio file. Called once recording is finished.
     */
    bool loadRecordedAudioFile();
    
    /**
     * @brief Retrieves all audio resources within the same subgroup.
     * @return A vector of shared pointers to `AudioResource` objects.
     */
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesWithinResourceGroup() const;
    
    /**
     * @brief Checks if the resource contains the specified absolute position.
     * @param position The position to check.
     * @param context The time context to use.
     * @return True if the position is within the resource, false otherwise.
     */
    bool containsAbsolutePosition(double position, audium::TimeContextType context) const;
    
    /**
     * @brief Writes the resource data to a JSON object.
     * @param output The JSON object to write to.
     * @return True if the data was successfully written, false otherwise.
     */
    bool writeToJson (json& output) override;
    
    /**
     * @brief Reads the resource data from a JSON object.
     * @param input The JSON object to read from.
     * @param rebuild Whether to rebuild the resource after reading.
     * @return True if the data was successfully read, false otherwise.
     */
    bool readFromJson (json& input, bool rebuild) override;
    
    /**
     * @brief Creates a URL from a JSON object.
     * @param input The JSON object containing the URL data.
     * @return The created `juce::URL`.
     */
    static const juce::URL urlFromJson (json& input);
    
    /**
     * @brief Tests the validity of a URL.
     * @param url The URL to test.
     */
    static void testUrl (const juce::URL& url);
    
    /**
     * @brief Retrieves the associated audio track.
     * @return A shared pointer to the `AudioTrack`.
     */
    std::shared_ptr<AudioTrack> getAudioTrack() const { return audioTrack; }
    
    /**
     * @brief Retrieves the associated resource group.
     * @return A shared pointer to the `ResourceGroup`.
     */
    std::shared_ptr<ResourceGroup> getResourceGroup() const { return resourceGroup; }
    
    /**
     * @brief Retrieves the channel mapping for the resource.
     * @return A reference to the `ChannelMapping`.
     */
    audium::ChannelMapping &getChannelMapping() const { return *channelMapping.get();}
    
    /**
     * @brief A shared pointer to the `AudioFormatReader` for the audio file.
     */
    std::shared_ptr<juce::AudioFormatReader> audioFormatReader;
    
    
    bool isRecording() const { return audioFormatReader == nullptr; }
    
    double getRecordedLength(audium::TimeContextType context) const;
    
private:
    /**
     * @brief The container managing this resource.
     */
    AudioResourceContainer& owner;

    /**
     * @brief A shared pointer to the associated `AudioTrack`.
     */
    std::shared_ptr<AudioTrack> audioTrack;

    /**
     * @brief A shared pointer to the associated `ResourceGroup`.
     */
    std::shared_ptr<ResourceGroup> resourceGroup;

    /**
     * @brief The URL of the audio file.
     */
    juce::URL url;

    /**
     * @brief A unique pointer to the channel mapping for the resource.
     */
    std::unique_ptr<audium::ChannelMapping> channelMapping;

    /**
     * @brief The length of the audio file in seconds.
     */
    double lengthInSeconds = 0.0;

    /**
     * @brief The number of channels in the audio file.
     */
    int numChannels = 1;

private:
    /**
     * @brief JUCE macro to prevent copying and detect memory leaks.
     */
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioResource)
};

} // namespace audium
