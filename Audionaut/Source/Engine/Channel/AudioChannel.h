//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Engine/Resource/AudioResource.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Channel/AudioChannelData.h"
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/Playback/AudioBusInterface.h"

namespace audium
{

/**
 * @class AudioChannel
 * @brief Represents an audio channel within an audio track.
 *
 * This class provides functionality for managing the properties of an audio channel,
 * such as height, gain, pan, mute, and solo states. It also integrates with the
 * selection system and audio bus interface for advanced audio processing.
 */
class AudioChannel : public audium::Selectable
{
    
public:
    /**
     * @brief Constructs an AudioChannel instance.
     * @param audioTrack_ Reference to the associated audio track.
     * @param selectionManager_ Shared pointer to the selection manager.
     * @param audioBusInterface_ Shared pointer to the audio bus interface.
     */
    AudioChannel(AudioTrack &audioTrack_,
                 std::shared_ptr<SelectionManager> selectionManager_,
                 std::shared_ptr<AudioBusInterface> audioBusInterface_) :
    audium::Selectable(selectionManager_),
    audioTrack(audioTrack_),
    audioBusInterface(audioBusInterface_)
    {
    }

    /**
    * @brief Retrieves the height of the audio channel.
    * @return The height of the audio channel in pixels.
    */
    int getChannelHeight() const noexcept {
        return data.height;
    }
    
    /**
     * @brief Sets the height of the audio channel.
     * @param height The new height of the audio channel in pixels.
     */
    void setChannelHeight(const int height) {
        data.height = height;
    }

   /**
    * @brief Sets the gain level of the audio channel.
    * @param new_gain The new gain level.
    */
   void setGain(const float new_gain);

   /**
    * @brief Retrieves the gain level of the audio channel.
    * @return The current gain level.
    */
   float getGain() const noexcept;

   /**
    * @brief Sets the pan position of the audio channel.
    * @param new_pan The new pan position (-1.0f for left, 1.0f for right).
    */
   void setPan(const float new_pan);

   /**
    * @brief Retrieves the pan position of the audio channel.
    * @return The current pan position.
    */
   float getPan() const noexcept;

   /**
    * @brief Sets the mute state of the audio channel.
    * @param bMute True to mute the channel, false to unmute.
    */
   void setMute(bool bMute);

   /**
    * @brief Retrieves the mute state of the audio channel.
    * @return True if the channel is muted, false otherwise.
    */
   bool getMute() const noexcept;

   /**
    * @brief Sets the solo state of the audio channel.
    * @param bSolo True to solo the channel, false to unsolo.
    */
   void setSolo(bool bSolo);

   /**
    * @brief Retrieves the solo state of the audio channel.
    * @return True if the channel is in solo mode, false otherwise.
    */
   bool getSolo() const noexcept;

   /**
    * @brief Commits the current channel data to the associated track.
    */
   void commitChannelData();
    
    int getChannelNumber() const {
        auto channel = std::dynamic_pointer_cast<const AudioChannel>(getSharedPtr());
        return audioTrack.audioChannelContainer->getIndex(channel);
    }
    
    AudioTrack &getAudioTrack() const { return audioTrack; }
    
    AudioChannelData data; ///< The data structure holding the channel's properties.
    
private:
    AudioTrack &audioTrack; ///< Reference to the associated audio track.
    std::shared_ptr<AudioBusInterface> audioBusInterface; ///< Shared pointer to the audio bus interface.
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioChannel)
    
};

} // namespace audium

