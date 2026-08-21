//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.


#pragma once

#include <JuceHeader.h>
#include "ClipDynamicsProcessor.h"
#include "VolumeFade.h"

using namespace juce;

namespace audium {

class AudioTransportSource;

//==============================================================================
/**
 * @struct ClipFadeSpec
 * @brief The complete fade geometry of one clip, in seconds, file/source time
 *        domain. One spec drives live scheduling, project bounce and item
 *        bounce identically.
 *
 * The signed offsets follow ClipDynamics: positive = ramp boundary inside
 * the clip (silent head/tail), negative = outside (the fade extends the
 * audible material past the region window).
 */
struct ClipFadeSpec
{
    double regionStart = 0.0, regionEnd = 0.0;   // the region window
    double fadeIn = 0.0, fadeOut = 0.0;          // ramp end from start / ramp start from end
    double fadeInStart = 0.0, fadeOutEnd = 0.0;  // signed ramp offsets
    double fadeInCurve = 0.5, fadeOutCurve = 0.5; // curve exponents (0.5 = equal power)

    double headExtension()  const noexcept { return juce::jmax(0.0, -fadeInStart); }
    double tailExtension()  const noexcept { return juce::jmax(0.0, -fadeOutEnd); }

    /** Portion of the head extension before the source file's first sample -
        the voice cannot render it; it is timeline silence. */
    double preFileSilence() const noexcept { return juce::jmax(0.0, headExtension() - regionStart); }

    /** First file position the voice reads from; never negative. */
    double voiceFileStart() const noexcept { return regionStart - headExtension() + preFileSilence(); }

    /** File position the voice reads to (past EOF renders silent). */
    double voiceFileEnd()   const noexcept { return regionEnd + tailExtension(); }

    /** Full audible length including both extensions. */
    double audibleLength()  const noexcept { return (regionEnd - regionStart) + headExtension() + tailExtension(); }
};

/**
 * Configures both clip fades on `source` for a voice scheduled to read from
 * filePositionSeconds (>= spec.voiceFileStart()). The one shared fade
 * configuration used by PlayListScheduler::scheduleClip and
 * AudiumTransportSource::configureDynamics - kept in lockstep by
 * construction. Real-time safe.
 */
void configureClipFades (AudioTransportSource& source,
                         const ClipFadeSpec& spec,
                         double filePositionSeconds,
                         bool reset);

//==============================================================================
/**
    An AudioSource that takes a PositionableAudioSource and allows it to be
    played, stopped, started, etc.

    This can also be told use a buffer and background thread to read ahead, and
    if can correct for different sample-rates.

    You may want to use one of these along with an AudioSourcePlayer and AudioIODevice
    to control playback of an audio file.

    @see AudioSource, AudioSourcePlayer

    @tags{Audio}
*/
class AudioTransportSource  : public PositionableAudioSource
{
public:
    //==============================================================================
    /** Creates an AudioTransportSource.
        After creating one of these, use the setSource() method to select an input source.
    */
    /** The dynamics processor (clip gain + fades) is injected so the
        rendering chain can be configured and tested on its own. */
    explicit AudioTransportSource (std::shared_ptr<ClipDynamicsProcessor> dynamicsProcessor_
                                       = std::make_shared<ClipDynamicsProcessor>());

    /** Destructor. */
    ~AudioTransportSource() override;

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
    void setSource (PositionableAudioSource* newSource,
                    int readAheadBufferSize = 0,
                    TimeSliceThread* readAheadThread = nullptr,
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

    void start();
    void stop(bool fadeout);

    /** Returns true if it's currently playing. */
    bool isPlaying() const noexcept     { return playing; }

    /** Returns true if it's stopped. */
    bool isStopped() const noexcept     { return stopped; }
    

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

    /** The injected dynamics chain, for callers that configure it directly. */
    std::shared_ptr<ClipDynamicsProcessor> getDynamicsProcessor() const { return dynamicsProcessor; }

    //==============================================================================
    /** Implementation of the AudioSource method. */
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;

    /** Implementation of the AudioSource method. */
    void releaseResources() override;

    /** Implementation of the AudioSource method. */
    void getNextAudioBlock (const AudioSourceChannelInfo&) override;

    //==============================================================================
    /** Implements the PositionableAudioSource method. */
    void setNextReadPosition (int64 newPosition) override;

    /** Implements the PositionableAudioSource method. */
    int64 getNextReadPosition() const override;

    /** Implements the PositionableAudioSource method. */
    int64 getTotalLength() const override;

    /** Implements the PositionableAudioSource method. */
    bool isLooping() const override;

    BufferingAudioSource* getBufferingSource() const { return bufferingSource; }
    
private:
    //==============================================================================
    PositionableAudioSource* source = nullptr;
    ResamplingAudioSource* resamplerSource = nullptr;
    BufferingAudioSource* bufferingSource = nullptr;
    PositionableAudioSource* positionableSource = nullptr;
    AudioSource* masterSource = nullptr;

    std::shared_ptr<ClipDynamicsProcessor> dynamicsProcessor;

    std::atomic<bool> playing  = false;
    std::atomic<bool> stopped  = true;
    std::atomic<bool> fadeOutLastBlock  = false;
    double sampleRate = 44100.0, sourceSampleRate = 0.0;
    int blockSize = 128, readAheadBufferSize = 0;
    std::atomic<bool> isPrepared = false;

    
    
    void releaseMasterResources();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTransportSource)
};

} // namespace audium
