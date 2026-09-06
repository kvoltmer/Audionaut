//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/Playback/AudioBusRenderer.h"
#include "Engine/Core/LockFreeCommander.h"
#include "Engine/Channel/AudioChannelData.h"

namespace audium
{
    class AudioThumbnail;

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

    template <typename ProcessContext>
    void process (const ProcessContext& context) noexcept
    {
        lockFreeCommander->invoke();
        audioBusRenderer->process(context);
    }

    
    void invokeCommands () noexcept
    {
        lockFreeCommander->invoke();
    }
    
    /**
    * @brief Sets the number of audio bus channels.
    * @param numChannels The number of channels to set.
    */
    void setNumAudioBusChannels(int numChannels);

    /**
    * @brief Sets the channel data for a specific channel.
    */
    void setChannelData(const int channelNumber, const AudioChannelData data);
    
    const AudioChannelData getChannelData(const int channelNumber) const;

    
    void setRecordEnabled(const int channelNumber,
                          bool bEnabled);
        
    std::shared_ptr<audium::AudioThumbnail> getRecordingThumbnail(int channelNumber) const;
    
    void record(bool start, const int channelNumber = -1, const double positionClocks = -1.0);
    
    const juce::File getRecordedAudioFile(int channelNumber);
    
    const double getRecordedLength(int channelNumber) const;
    
    const double getRecordingStartPosition(int channelNumber) const;
    
    bool isRecording(int channelNumber) const;
    
    bool anyChannelRecording() const;
    
    /**
    * @brief Sets the master gain for the audio bus.
    * @param newGain The new master gain value.
    */
    void setMasterGain(const float newGain);
    
    void resetGains();

    void stopAllVoices();

    /**
    * @brief Enables/disables the offline bounce mapping (identity bus -> output copy,
    * bypassing output routing). Call only while the live device callback is bypassed.
    */
    void setStemExport(bool bStemExport);

    /**
    * @brief Gets the current level of a specific channel.
    * @param channelNumber The channel index.
    * @return The current level of the channel.
    */
    const float getChannelLevel(const int channelNumber) const;

    /**
    * @brief Gets the current level of a recording enabled channel.
    * @param channelNumber The channel index.
    * @return The current level of the channel.
    */
    const float getRecordingLevel(const int channelNumber) const;
    
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
