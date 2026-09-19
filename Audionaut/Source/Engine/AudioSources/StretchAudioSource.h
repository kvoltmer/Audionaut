//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <JuceHeader.h>

#include "Engine/AudioSources/Stretch/RubberBandStretchBackend.h"

namespace audium {

//==============================================================================
/**
    Pitch-preserving time-stretch node for the clip playback chain.

    Sits behind ClipTransportSource's resampler the way the class comment on
    setSpeedRatio always promised: in Stretch mode the resampler only
    corrects the file's sample rate and this node realises the speed ratio -
    consuming speedRatio input samples per output sample - while the pitch
    stays put. In RePitch mode the node is bypassed (a plain pass-through)
    and the resampler does the varispeed as before.

    The stretching itself is Rubber Band's R3 engine, wrapped by
    RubberBandStretchBackend, which also owns the start alignment.

    The node lives in the chain permanently and is toggled per voice: the
    scheduler sets mode and speed before it schedules the position, and a
    position change re-primes on the next rendered block. Priming hands the
    backend look-ahead input so the very first process() output is aligned
    with the scheduled position - no latency reaches the timeline.

    Real-time notes: after prepareToPlay nothing here allocates; upstream
    input is pulled in prepared-block-size chunks so the resampler and
    buffering sources never see a larger request than they prepared for.

    @see ClipTransportSource, StretchMode, RubberBandStretchBackend
*/
class StretchAudioSource : public juce::AudioSource
{
public:
    /// Wraps @p inputSource (not owned). @p numChannels is the channel
    /// count of the upstream chain (the file reader's).
    StretchAudioSource (juce::AudioSource* inputSource, int numChannels);
    ~StretchAudioSource() override;

    /// Bypassed (the default) means pass-through: RePitch mode.
    void setEnabled (bool shouldStretch) noexcept    { enabled.store (shouldStretch); }
    bool isEnabled() const noexcept                  { return enabled.load(); }

    /// Output-domain speed: consumes newSpeedRatio input samples per output
    /// sample. Real-time safe; a change mid-voice glides without a click.
    void setSpeedRatio (double newSpeedRatio) noexcept   { speedRatio.store (newSpeedRatio); }

    /// Forget everything buffered and re-prime on the next block - call
    /// after the upstream read position changed.
    void flushBuffers() noexcept                     { needsPriming.store (true); }

    /**
        Lets the owner say whether @p numInputSamples (in this node's input
        domain) can be pulled from upstream right now, and how much
        look-ahead upstream can hold at all. While the answer is no, a
        pending prime is deferred: the block plays silent and nothing is
        consumed, and once the input is there the deferred span is skipped
        so the clip stays where the timeline put it. Without a probe the
        node pulls unconditionally.
    */
    void setInputReadiness (std::function<bool (int numInputSamples)> isReady,
                            std::function<int()> maxLookAhead);

    /// Diagnostics: blocks the stretcher could not fill, and rendered
    /// samples it had to drop. Both stay zero when the buffers are sized
    /// right.
    int getUnderruns() const noexcept    { return backend.getUnderruns(); }
    int getOverflows() const noexcept    { return backend.getOverflows(); }

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

    RubberBandStretchBackend backend;
    bool prepared = false;

    std::atomic<double> speedRatio { 1.0 };
    std::atomic<bool> enabled { false };
    std::atomic<bool> needsPriming { true };

    juce::AudioBuffer<float> inputScratch;
    int preparedBlockSize = 0;

    std::function<bool (int)> inputReady;
    std::function<int()> maxLookAhead;
    int deferredOutputSamples = 0;   // silent blocks played while a prime waited for input

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StretchAudioSource)
};

} // namespace audium
