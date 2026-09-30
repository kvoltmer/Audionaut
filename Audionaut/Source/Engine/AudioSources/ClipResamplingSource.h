//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.


#pragma once

#include <JuceHeader.h>

namespace audium {

//==============================================================================
/**
    The clip chain's sample-rate converter: reads its input at a ratio of
    input samples per output sample (the file's rate over the device rate,
    times the varispeed).

    Forked from juce::ResamplingAudioSource with one deliberate change.
    The original sizes its ring buffer in prepareToPlay from the ratio
    current at that moment and grows it inside getNextAudioBlock as soon
    as a later ratio needs more - a heap allocation on the audio thread.
    A clip's ratio does move after prepareToPlay: the transport applies
    the file-rate correction once the chain is prepared, varispeed and
    tempo follow change it while the clip plays, and a standby lane gets
    its ratio at prime time. So this fork takes an upper bound,
    setMaximumResamplingRatio, and sizes the buffer for it. Within that
    bound, and for blocks up to the prepared size, getNextAudioBlock never
    allocates; the growth path stays as a guarded fallback that is counted
    (getBufferGrowths) so tests can pin it at zero.

    @see ClipTransportSource
*/
class ClipResamplingSource  : public juce::AudioSource
{
public:
    //==============================================================================
    /** Creates a ClipResamplingSource for a given input source.
        @param inputSource              the input source to read from
        @param deleteInputWhenDeleted   if true, the input source will be deleted when
                                        this object is deleted
        @param numChannels              the number of channels to process
    */
    ClipResamplingSource (juce::AudioSource* inputSource,
                          bool deleteInputWhenDeleted,
                          int numChannels = 2);

    /** Destructor. */
    ~ClipResamplingSource() override;

    //==============================================================================
    /** Changes the resampling ratio.

        (This value can be changed at any time, even while the source is running).

        @param samplesInPerOutputSample     if set to 1.0, the input is passed through; higher
                                            values will speed it up; lower values will slow it
                                            down. The ratio must be greater than 0
    */
    void setResamplingRatio (double samplesInPerOutputSample);

    /** Returns the current resampling ratio.
        This is the value that was set by setResamplingRatio().
    */
    double getResamplingRatio() const noexcept                  { return ratio; }

    /** The largest ratio getNextAudioBlock has to serve without growing
        its buffer. prepareToPlay sizes the buffer for the greater of this
        and the current ratio, so set it before preparing, off the audio
        thread; a ratio above it still plays but falls back to growing the
        buffer in the callback. */
    void setMaximumResamplingRatio (double maximumSamplesInPerOutputSample);

    double getMaximumResamplingRatio() const noexcept           { return maximumRatio; }

    /** Clears any buffers and filters that the resampler is using. */
    void flushBuffers();

    /** The ring buffer's length in samples, fixed by prepareToPlay. */
    int getBufferSize() const noexcept                          { return buffer.getNumSamples(); }

    /** How often getNextAudioBlock had to grow the buffer after all: zero
        unless a ratio beyond the maximum, or a block beyond the prepared
        size, came through. */
    int getBufferGrowths() const noexcept                       { return bufferGrowths; }

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo&) override;

private:
    //==============================================================================
    juce::OptionalScopedPointer<juce::AudioSource> input;
    double ratio = 1.0, lastRatio = 1.0, maximumRatio = 1.0;
    juce::AudioBuffer<float> buffer;
    int bufferPos = 0, sampsInBuffer = 0;
    int bufferGrowths = 0;
    double subSampleOffset = 0.0;
    double coefficients[6];
    juce::SpinLock ratioLock;
    juce::CriticalSection callbackLock;
    const int numChannels;
    juce::HeapBlock<float*> destBuffers;
    juce::HeapBlock<const float*> srcBuffers;

    void setFilterCoefficients (double c1, double c2, double c3, double c4, double c5, double c6);
    void createLowPass (double proportionalRate);

    struct FilterState
    {
        double x1, x2, y1, y2;
    };

    juce::HeapBlock<FilterState> filterStates;
    void resetFilters();
    void applyFilter (float* samples, int num, FilterState& fs);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipResamplingSource)
};

} // namespace audium
