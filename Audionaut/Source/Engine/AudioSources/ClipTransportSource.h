//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.


#pragma once

#include <JuceHeader.h>
#include "ClipDynamicsProcessor.h"
#include "StretchAudioSource.h"
#include "Engine/PlayList/StretchMode.h"

namespace audium {

//==============================================================================
/**
    The transport for one clip: takes a PositionableAudioSource and plays,
    stops and repositions it, applying the clip's gain and fade ramps.

    Forked from juce::AudioTransportSource with three deliberate changes:
    the ChangeBroadcaster base is gone, the flat gain is replaced by an
    injected ClipDynamicsProcessor (clip gain + fade-in/out ramps, applied
    post-resampling), and the callback lock is removed. The threading
    contract without that lock: configuration (setSource, prepareToPlay)
    happens off the audio thread and is gated by the atomic isPrepared
    flag; setSource must not race an in-flight getNextAudioBlock.

    @see VoiceSource, ClipDynamicsProcessor, ClipFadeSpec
*/
class ClipTransportSource  : public juce::PositionableAudioSource
{
public:
    //==============================================================================
    /** Creates a ClipTransportSource.
        After creating one of these, use the setSource() method to select an input source.
        The dynamics processor (clip gain + fades) is injected so the
        rendering chain can be configured and tested on its own. */
    explicit ClipTransportSource (std::shared_ptr<ClipDynamicsProcessor> dynamicsProcessor_
                                       = std::make_shared<ClipDynamicsProcessor>());

    /** Destructor. */
    ~ClipTransportSource() override;

    //==============================================================================
    /** Sets the reader that is being used as the input source.

        This will stop playback, reset the position to 0 and change to the new reader.

        The source passed in will not be deleted by this object, so must be managed by
        the caller.

        @param newSource                        the new input source to use. This may be a nullptr
        @param readAheadBufferSize              a size of buffer to use for reading ahead. If this
                                                is zero, no reading ahead will be done; if it's
                                                greater than zero, a BufferingAudioSource will be used
                                                to do the reading-ahead. If you set a non-zero value here,
                                                you'll also need to set the readAheadThread parameter.
        @param readAheadThread                  if you set readAheadBufferSize to a non-zero value, then
                                                you'll also need to supply this TimeSliceThread object for
                                                the background reader to use. The thread object must not be
                                                deleted while the AudioTransport source is still using it.
        @param sourceSampleRateToCorrectFor     if this is non-zero, it specifies the sample
                                                rate of the source, and playback will be sample-rate
                                                adjusted to maintain playback at the correct pitch. If
                                                this is 0, no sample-rate adjustment will be performed
        @param maxNumChannels                   the maximum number of channels that may need to be played
    */
    void setSource (juce::PositionableAudioSource* newSource,
                    int readAheadBufferSize = 0,
                    juce::TimeSliceThread* readAheadThread = nullptr,
                    double sourceSampleRateToCorrectFor = 0.0,
                    int maxNumChannels = 2);

    //==============================================================================
    /** Changes the current playback position in the source stream.

        The next time the getNextAudioBlock() method is called, this
        is the time from which it'll read data.

        @param newPosition    the new playback position in seconds

        @see getCurrentPosition
    */
    void setPosition (double newPosition);

    /** Returns the position that the next data block will be read from.
        This is a time in seconds.
    */
    double getCurrentPosition() const;

    /** Returns the stream's length in seconds. */
    double getLengthInSeconds() const;

    /** Returns true if the player has stopped because its input stream ran out of data. */
    bool hasStreamFinished() const noexcept;

    /** Starts playback (no-op while already playing or without a source). */
    void start();

    /** Stops playback. With fadeout, the next rendered block ramps its
        first 256 samples to silence instead of cutting hard. */
    void stop(bool fadeout);

    /** Returns true if it's currently playing. */
    bool isPlaying() const noexcept     { return playing; }

    /** Returns true if it's stopped. */
    bool isStopped() const noexcept     { return stopped; }

    /**
        Sets the clip's playback speed. How it is realised depends on the
        stretch mode: RePitch is varispeed (the resampling ratio becomes
        sourceSampleRate * speed / deviceRate), Stretch keeps the pitch by
        running the StretchAudioSource node at the speed while the
        resampler only corrects the file's sample rate. Real-time safe.
    */
    void setSpeedRatio (double newSpeedRatio) noexcept;

    /**
        Chooses between varispeed (RePitch, the default) and
        pitch-preserving Stretch - see setSpeedRatio. Real-time safe; the
        scheduler sets it per voice before scheduling the position, and the
        stretch node re-primes on the position change.
    */
    void setStretchMode (StretchMode newMode) noexcept;

    void setGain (float newGain) noexcept;
    float getGain() const noexcept;
    
    void resetClipGain();
    
    /** Transparent resets: no fade, no stale skip/silent counters. */
    void clearFadeIn();
    void clearFadeOut();

    /** The curve exponent applied over each ramp (ClipDynamics::fadeCurve). */
    void setFadeInCurve(double curve);
    void setFadeOutCurve(double curve);

    /** Configures the fade-in as a ramp of rampSeconds starting
        rampStartSeconds after the scheduled position. rampStartSeconds > 0
        emits silence until the ramp starts; <= 0 means |value| of the ramp
        has already elapsed (the ramp is armed mid-way). */
    void setFadeInRamp(double rampSeconds, double rampStartSeconds, bool reset);

    /** Configures the fade-out as a ramp of rampSeconds starting
        rampStartSeconds after the scheduled position; the completed ramp
        holds silence. rampSeconds <= 0 with a positive start is an instant
        mute at the ramp point. rampStartSeconds as in setFadeInRamp (> 0:
        unity pass-through until then). */
    void setFadeOutRamp(double rampSeconds, double rampStartSeconds, bool reset);

    //==============================================================================
    /** Implementation of the AudioSource method. */
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;

    /** Implementation of the AudioSource method. */
    void releaseResources() override;

    /** Implementation of the AudioSource method. */
    void getNextAudioBlock (const juce::AudioSourceChannelInfo&) override;

    //==============================================================================
    /** Implements the PositionableAudioSource method. */
    void setNextReadPosition (juce::int64 newPosition) override;

    /** Implements the PositionableAudioSource method. */
    juce::int64 getNextReadPosition() const override;

    /** Implements the PositionableAudioSource method. */
    juce::int64 getTotalLength() const override;

    /** Implements the PositionableAudioSource method. */
    bool isLooping() const override;

    juce::BufferingAudioSource* getBufferingSource() const { return bufferingSource; }

private:
    //==============================================================================
    juce::PositionableAudioSource* source = nullptr;
    /// Re-applies mode and speed to the resampler and the stretch node.
    void updateSpeedChain() noexcept;

    juce::ResamplingAudioSource* resamplerSource = nullptr;
    StretchAudioSource* stretchSource = nullptr;
    juce::BufferingAudioSource* bufferingSource = nullptr;
    juce::PositionableAudioSource* positionableSource = nullptr;
    juce::AudioSource* masterSource = nullptr;

    std::shared_ptr<ClipDynamicsProcessor> dynamicsProcessor;

    std::atomic<bool> playing  = false;
    std::atomic<bool> stopped  = true;
    std::atomic<bool> fadeOutLastBlock  = false;
    double sampleRate = 44100.0, sourceSampleRate = 0.0;
    std::atomic<double> speedRatio { 1.0 };
    std::atomic<StretchMode> stretchMode { StretchMode::RePitch };
    int blockSize = 128;
    std::atomic<bool> isPrepared = false;

    void releaseMasterResources();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipTransportSource)
};

} // namespace audium
