//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/Playback/AudioBusRenderer.h"
#include "Engine/Core/LockFreeCommander.h"

namespace audium
{

/**
 * @brief Constructs an `AudioBusInterface` instance.
 * @param lockFreeCommander_ Shared pointer to the lock-free commander for thread-safe operations.
 * @param audioBusRenderer_ Shared pointer to the audio bus renderer for audio processing.
 */
class AudioBusInterface
{
    
public:
    /**
     * @brief Constructs an `AudioBusInterface` instance.
     * @param lockFreeCommander_ Shared pointer to the lock-free commander for thread-safe operations.
     * @param audioBusRenderer_ Shared pointer to the audio bus renderer for audio processing.
     */
    AudioBusInterface(std::shared_ptr<audium::LockFreeCommander> lockFreeCommander_,
                      std::shared_ptr<audium::AudioBusRenderer<float>> audioBusRenderer_) :
        lockFreeCommander(lockFreeCommander_),
        audioBusRenderer(audioBusRenderer_)
    {
    }
    
    /**
      * @brief Default destructor for `AudioBusInterface`.
      */
     ~AudioBusInterface() = default;

     /**
      * @brief Prepares the audio bus for playback.
      * @param samplesPerBlockExpected The expected number of samples per block.
      * @param sampleRate The sample rate for audio processing.
      */
     void prepareToPlay(int samplesPerBlockExpected, double sampleRate);

     /**
      * @brief Processes audio data and outputs it to the specified channel info.
      * @param outputInfo The `juce::AudioSourceChannelInfo` containing output buffer details.
      */
     void processAudio(const juce::AudioSourceChannelInfo& outputInfo);

     /**
      * @brief Sets the number of audio bus channels.
      * @param numChannels The number of channels to set.
      */
     void setNumAudioBusChannels(int numChannels);

     /**
      * @brief Sets the pan value for a specific channel.
      * @param channelNumber The channel index.
      * @param newPan The new pan value (-1.0 for full left, 1.0 for full right).
      */
     void setPan(const int channelNumber, const float newPan);

     /**
      * @brief Sets the gain value for a specific channel.
      * @param channelNumber The channel index.
      * @param newGain The new gain value.
      */
     void setGain(const int channelNumber, const float newGain);

     /**
      * @brief Mutes or unmutes a specific channel.
      * @param channelNumber The channel index.
      * @param bMute True to mute the channel, false to unmute.
      */
     void setMute(const int channelNumber, const bool bMute);

     /**
      * @brief Solos or unsolos a specific channel.
      * @param channelNumber The channel index.
      * @param bSolo True to solo the channel, false to unsolo.
      */
     void setSolo(const int channelNumber, const bool bSolo);

     /**
      * @brief Sets the master gain for the audio bus.
      * @param newGain The new master gain value.
      */
     void setMasterGain(const float newGain);

     /**
      * @brief Gets the current level of a specific channel.
      * @param channelNumber The channel index.
      * @return The current level of the channel.
      */
     const float getChannelLevel(const int channelNumber) const;

     /**
      * @brief Gets the current master level for a specific channel.
      * @param channelNumber The channel index.
      * @return The current master level of the channel.
      */
     const float getMasterLevel(const int channelNumber) const;

 private:
     std::shared_ptr<audium::LockFreeCommander> lockFreeCommander; ///< Thread-safe command manager.
     std::shared_ptr<audium::AudioBusRenderer<float>> audioBusRenderer; ///< Renderer for audio bus processing.

     JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioBusInterface)
 };

 } // namespace audium
