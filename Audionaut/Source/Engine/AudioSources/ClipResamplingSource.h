//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

namespace audium {

//==============================================================================
/**
    The clip chain's resampler: linear interpolation with a second-order
    anti-aliasing low-pass, forked from juce::ResamplingAudioSource for
    one reason - a seek that does not click.

    juce::ResamplingAudioSource can only seek by flushing: its filter state
    is zeroed and its interpolation phase reset, both private. Every seek
    (a loop wrap in particular) then starts with the filter's step
    response, and lands on a whole source sample. This fork seeks to a
    fractional source position and pre-rolls a few output samples before
    the target so that the filter is primed with the material that
    actually precedes it: the output after the seek is what an unbroken
    stream would have produced there.

    At a ratio of 1 (no rate correction, no speed) the filter is off and
    the seek snaps to the nearest sample, so same-rate playback stays
    bit-exact.

    @see ClipTransportSource
*/
class ClipResamplingSource : public juce::AudioSource
{
public:
    /** The input must be positionable, since a seek repositions it.
        It is not owned. */
    ClipResamplingSource (juce::PositionableAudioSource* inputSource, int numChannels = 2);

    ~ClipResamplingSource() override;

    /** Source samples consumed per output sample (sourceRate / outputRate,
        times the clip's speed for varispeed). Real-time safe. */
    void setResamplingRatio (double samplesInPerOutputSample);

    double getResamplingRatio() const noexcept          { return ratio.load(); }

    /** Repositions the stream: the next output sample interpolates the
        input at this (fractional) source sample. Primes the filter from
        the preceding source material - see the class comment. Called from
        the render thread. */
    void setNextReadPosition (double sourceSamplePosition);

    /** Drops the buffered input and the filter state without repositioning
        the input (the input was repositioned by someone else). */
    void flushBuffers();

    /** Output samples rendered and discarded before a seek target to prime
        the filter; the step response of the low-pass settles well within. */
    static constexpr int preRollSamples = 32;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo&) override;

private:
    //==============================================================================
    juce::PositionableAudioSource* input;
    const int numChannels;
    std::atomic<double> ratio { 1.0 };
    double lastRatio = 1.0;
    juce::AudioBuffer<float> buffer;
    juce::AudioBuffer<float> preRollBuffer;
    int bufferPos = 0, sampsInBuffer = 0;
    double subSampleOffset = 0.0;
    double coefficients[6];

    struct FilterState
    {
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
    };

    juce::HeapBlock<FilterState> filterStates;
    juce::HeapBlock<const float*> srcBuffers;
    juce::HeapBlock<float*> destBuffers;

    /** Whether the low-pass takes part at this ratio (juce's thresholds). */
    static bool isFiltering (double localRatio) noexcept
    {
        return localRatio > 1.0001 || localRatio < 0.9999;
    }

    void resetState();
    void createLowPass (double proportionalRate);
    void setFilterCoefficients (double c1, double c2, double c3, double c4, double c5, double c6);
    void resetFilters();
    void applyFilter (float* samples, int num, FilterState&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipResamplingSource)
};

} // namespace audium
