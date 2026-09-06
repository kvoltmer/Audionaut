//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <JuceHeader.h>

#include <signalsmith-stretch/signalsmith-stretch.h>

namespace audium {

//==============================================================================
/**
    Pitch-preserving time-stretch node (signalsmith-stretch) for the clip
    playback chain.

    Sits behind ClipTransportSource's resampler the way the class comment on
    setSpeedRatio always promised: in Stretch mode the resampler only
    corrects the file's sample rate and this node realises the speed ratio -
    consuming speedRatio input samples per output sample - while the pitch
    stays put. In RePitch mode the node is bypassed (a plain pass-through)
    and the resampler does the varispeed as before.

    The node lives in the chain permanently and is toggled per voice: the
    scheduler sets mode and speed before it schedules the position, and a
    position change re-primes on the next rendered block. Priming uses the
    library's outputSeek(), which seeks and pre-rolls internally so the very
    first process() output is aligned with the scheduled position - no
    latency reaches the timeline.

    Real-time notes: process() and outputSeek() are allocation-free after
    prepareToPlay pre-warms the library's internal buffers; upstream input
    is pulled in prepared-block-size chunks so the resampler and buffering
    sources never see a larger request than they prepared for.

    @see ClipTransportSource, StretchMode
*/
class StretchAudioSource : public juce::AudioSource
{
public:
    /// Wraps @p inputSource (not owned). @p numChannels is the channel
    /// count of the upstream chain (the file reader's).
    StretchAudioSource (juce::AudioSource* inputSource, int numChannels);

    /// Bypassed (the default) means pass-through: RePitch mode.
    void setEnabled (bool shouldStretch) noexcept    { enabled.store (shouldStretch); }
    bool isEnabled() const noexcept                  { return enabled.load(); }

    /// Output-domain speed: consumes newSpeedRatio input samples per output
    /// sample. Real-time safe; a change mid-voice glides without a click
    /// (the library infers the rate per block).
    void setSpeedRatio (double newSpeedRatio) noexcept   { speedRatio.store (newSpeedRatio); }

    /// Forget everything buffered and re-prime on the next block - call
    /// after the upstream read position changed.
    void flushBuffers() noexcept                     { needsPriming.store (true); }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override;

private:
    /// Fills the first @p numSamples of inputScratch from upstream, pulling
    /// in chunks no larger than the prepared block size.
    void pullInput (int numSamples);

    void prime (double ratio);

    juce::AudioSource* input;
    const int numChannels;

    signalsmith::stretch::SignalsmithStretch<float> stretch;

    std::atomic<double> speedRatio { 1.0 };
    std::atomic<bool> enabled { false };
    std::atomic<bool> needsPriming { true };

    juce::AudioBuffer<float> inputScratch;
    double inputRemainder = 0.0;
    int preparedBlockSize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StretchAudioSource)
};

} // namespace audium
