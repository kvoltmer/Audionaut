//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Engine/Group/AudioClipData.h"
#include "Engine/Group/ResourceGroup.h"
#include "Engine/Streamable.h"
#include "Engine/PlayList/PositionableBase.h"

namespace audium {

/**
 * @class AudioClip
 * @brief Represents an audio clip within a resource group.
 *
 * The `AudioClip` class provides functionality for managing the position, region data,
 * and serialization of audio clips. It is associated with a `ResourceGroup` and supports
 * streaming and JSON-based serialization.
 */
class AudioClip : public audium::Streamable
{
public:
    /**
     * @brief Constructs an `AudioClip` instance.
     * @param resourceGroup Reference to the associated `ResourceGroup`.
     */
    AudioClip(ResourceGroup &resourceGroup) :
        resourceGroup(resourceGroup)
    {
    }

    /**
     * @brief Gets the absolute position of the audio clip.
     * @param context The time context type.
     * @return The absolute position of the clip.
     */
    double getAbsolutePosition(audium::TimeContextType context) const;

    /**
     * @brief Sets the absolute position of the audio clip.
     * @param position The new absolute position.
     * @param context The time context type.
     */
    void setAbsolutePosition(double position, audium::TimeContextType context);

    /**
     * @brief Gets the region data of the audio clip.
     * @param context The time context type.
     * @return The region data as a range.
     */
    juce::Range<double> getRegionData(audium::TimeContextType context) const;

    /**
     * @brief Sets the region data of the audio clip.
     * @param newRegionData The new region data as a range.
     * @param context The time context type.
     */
    void setRegionData(juce::Range<double> newRegionData, audium::TimeContextType context);

    /**
     * @brief Writes the audio clip data to a stream.
     * @param outputStream The output stream to write to.
     * @return True if the operation succeeds, false otherwise.
     */
    bool writeToStream(juce::OutputStream& outputStream) override;

    /**
     * @brief Reads the audio clip data from a stream.
     * @param inputStream The input stream to read from.
     * @param rebuild Whether to rebuild the clip during reading.
     * @return True if the operation succeeds, false otherwise.
     */
    bool readFromStream(juce::InputStream& inputStream, bool rebuild) override;

    /**
     * @brief Writes the audio clip data to a JSON object.
     * @param output The JSON object to write to.
     * @return True if the operation succeeds, false otherwise.
     */
    bool writeToJson(json& output) override;

    /**
     * @brief Reads the audio clip data from a JSON object.
     * @param input The JSON object to read from.
     * @param rebuild Whether to rebuild the clip during reading.
     * @return True if the operation succeeds, false otherwise.
     */
    bool readFromJson(json& input, bool rebuild) override;

    /**
     * @brief Gets the size of the audio clip in units.
     * @return The size of the clip in units.
     */
    int getSizeInUnits() override { return 1; }

    /**
     * @brief Validates the audio clip data.
     * @return True if the data is valid, false otherwise.
     */
    bool validateData();

    /**
     * @brief Gets the associated audio track.
     * @return Reference to the associated `AudioTrack`.
     */
    AudioTrack &getAudioTrack() const;

    /**
     * @brief Gets the file length of the audio clip.
     * @param context The time context type.
     * @return The file length of the clip.
     */
    double getFileLength(audium::TimeContextType context) const;

    AudioClipData data; ///< The data associated with the audio clip.

private:
    ResourceGroup &resourceGroup; ///< Reference to the associated resource group.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioClip)
};

} // namespace audium
