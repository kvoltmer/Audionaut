
#pragma once

#include <JuceHeader.h>
#include "VolumeFade.h"

using namespace juce;

namespace audium
{

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
    AudioTransportSource();

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
    void stop();

    /** Returns true if it's currently playing. */
    bool isPlaying() const noexcept     { return playing; }

    /** Returns true if it's stopped. */
    bool isStopped() const noexcept     { return stopped; }
    

    void setGain (float newGain) noexcept;
    float getGain() const noexcept;
    
    void setFadeInSeconds(double fadeInSeconds, double offsetInSeconds, bool reset);
    void setFadeOutSeconds(double fadeOutSeconds, double duration, bool reset);

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

    std::atomic<float> gain = 1.0f;

    std::atomic<bool> playing  = false;
    std::atomic<bool> stopped  = true;
    double sampleRate = 44100.0, sourceSampleRate = 0.0;
    int blockSize = 128, readAheadBufferSize = 0;
    std::atomic<bool> isPrepared = false;

    juce::dsp::Gain<float> clipGain;
    
    audium::VolumeFade<float> clipFadeIn;
    audium::VolumeFade<float> clipFadeOut;
    
    void releaseMasterResources();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTransportSource)
};

} // namespace audium
