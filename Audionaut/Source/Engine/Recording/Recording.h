//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "AudioRecorder.h"
#include "Engine/Playback/PlaybackDefines.h"

namespace audium {

/**
 * @brief Owns one `AudioRecorder` per armed channel.
 *
 * Recorders live on the message thread: arming constructs one (disk thread,
 * writer) and disarming retires it here, never in the audio callback. The
 * audio thread only ever sees a raw pointer per channel, handed over through
 * the lock-free commander (`publishRecorder`); a retired recorder is freed
 * on the message thread once the command that cleared its pointer has run.
 */
class Recording {

public:
    Recording();
    ~Recording();

    /**
     * @brief Global take counter: incremented by `AudioBusInterface::record`
     *        when a recording starts, read for naming the recorded file and
     *        its take region.
     */
    static int recordingCounter;

    void record(bool start, const int channelNumber, const double positionClocks);

    /**
     * @brief Arms or disarms a channel (message thread).
     *
     * Arming creates the channel's recorder, disarming stops and retires it.
     * The caller must push exactly one `publishRecorder` command with the
     * returned pointer afterwards: the retirement handshake counts on it.
     * @return The pointer to publish to the audio thread, null on disarm.
     */
    AudioRecorder* setRecordEnabled(const int channelNumber, bool bEnabled);

    /**
     * @brief Makes `recorder` the one the audio thread feeds for the channel
     *        (audio thread, via the commander) and acknowledges the command.
     */
    void publishRecorder(const int channelNumber, AudioRecorder* recorder) noexcept;

    /** The channel's armed recorder, if any (message thread). */
    std::shared_ptr<AudioRecorder> getAudioRecorder(int channelNumber) const;

    /** The channel's published recorder, if any (audio thread). */
    AudioRecorder* getAudioThreadRecorder(int channelNumber) const noexcept
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS)
            return audioThreadRecorders[channelNumber].load (std::memory_order_acquire);

        return nullptr;
    }

    const juce::File getRecordedFile(const int channelNumber) const
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS)
            return recordedFiles[channelNumber];
        
        return juce::File();
    }

    void setRecordedFile(const int channelNumber, const juce::File file)
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS)
            recordedFiles[channelNumber] = file;
    }
    
    void setSampleRate(double sampleRate_) { sampleRate = sampleRate_; }
        
    const double getRecordingStartPosition(int c) const { return recordingStartPositionClocks[c]; }
    
private:

    /** Destroys retired recorders whose publish command the audio thread has run (message thread). */
    void releaseRetiredRecorders();

    // message thread: the owning side
    std::shared_ptr<AudioRecorder> recorders[MAX_AUDIO_CHANNELS];

    // audio thread: what process() feeds, written by publishRecorder
    std::atomic<AudioRecorder*> audioThreadRecorders[MAX_AUDIO_CHANNELS] {};

    // Retirement handshake: every setRecordEnabled call is followed by one
    // publish command; a retired recorder may go once the command issued
    // with (or after) its disarm has been acknowledged by the audio thread.
    struct RetiredRecorder {
        std::shared_ptr<AudioRecorder> recorder;
        juce::uint64 ticket;
    };
    std::vector<RetiredRecorder> retiredRecorders;      // message thread
    juce::uint64 commandsIssued = 0;                    // message thread
    std::atomic<juce::uint64> commandsAcknowledged { 0 }; // audio thread -> message thread

    juce::File recordedFiles[MAX_AUDIO_CHANNELS];
    
    double recordingStartPositionClocks[MAX_AUDIO_CHANNELS];
    
    double sampleRate = 0.0;
    
    
    juce::AudioFormatManager formatManager;

    juce::AudioThumbnailCache audioThumbnailCache;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Recording)
};

} // namespace audium
