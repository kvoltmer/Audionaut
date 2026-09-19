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

    /** A standby lane: a second upstream chain (its own reader cursor and
        resampler, owned by the caller) with its own stretcher, so a known
        position jump - the loop wrap - can be primed ahead of time instead
        of inside the callback that makes the jump. Off the audio thread;
        the lane's stretcher is allocated in prepareToPlay (or here, when
        already prepared). */
    void setStandbyInput (juce::AudioSource* standbyInputSource);
    bool hasStandbyLane() const noexcept             { return standbyInput != nullptr; }

    /** Primes the standby lane for a position jump, one slice per call.
        The caller has already placed the standby input at the target;
        positionKey identifies that target (the source sample position),
        blocksLeft says how many more calls it can expect before the jump
        and sizes the slice so the prime completes one call early. A call
        with a new key restarts the prime; calls after completion are
        no-ops. Real-time safe. */
    void primeStandby (juce::int64 positionKey, double ratio, int blocksLeft);
    bool isStandbyPrimingFor (juce::int64 positionKey) const noexcept
    {
        return standby.active && standby.key == positionKey;
    }

    /** Makes the standby lane the live one if it is fully primed for
        positionKey: the lanes swap (input and stretcher), the next block
        renders from the primed state and no prime runs. Returns false -
        and changes nothing - otherwise; the caller then seeks and flushes
        as usual. Real-time safe. */
    bool adoptStandby (juce::int64 positionKey) noexcept;

    /// Full primes rendered inside a block, and jumps served by a standby.
    int getPrimeCount() const noexcept               { return primeCount; }
    int getStandbyAdoptions() const noexcept         { return standbyAdoptions; }

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
    int getUnderruns() const noexcept    { return backend->getUnderruns(); }
    int getOverflows() const noexcept    { return backend->getOverflows(); }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override;

private:
    /// Fills the first @p numSamples of inputScratch from @p from, pulling
    /// in chunks no larger than the prepared block size.
    void pullInput (juce::AudioSource& from, int numSamples);

    void prime (double ratio);

    juce::AudioSource* input;
    const int numChannels;

    // the live lane renders; the standby lane (input + backend, present
    // only with a standby input) is primed ahead of a jump and swapped in
    std::unique_ptr<RubberBandStretchBackend> backend;
    juce::AudioSource* standbyInput = nullptr;
    std::unique_ptr<RubberBandStretchBackend> standbyBackend;

    struct StandbyPrime
    {
        bool active = false;
        bool ready = false;
        juce::int64 key = 0;
        int padLeft = 0;      // start-pad zeros still to feed
        int inputLeft = 0;    // look-ahead input still to feed
        int slice = 0;        // samples (pad + input) fed per call
    };
    StandbyPrime standby;
    static constexpr int standbyMinSlice = 1024;

    int primeCount = 0;
    int standbyAdoptions = 0;
    bool prepared = false;

    std::atomic<double> speedRatio { 1.0 };
    std::atomic<bool> enabled { false };
    std::atomic<bool> needsPriming { true };

    juce::AudioBuffer<float> inputScratch;
    int preparedBlockSize = 0;
    double preparedSampleRate = 0.0;

    std::function<bool (int)> inputReady;
    std::function<int()> maxLookAhead;
    int deferredOutputSamples = 0;   // silent blocks played while a prime waited for input

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StretchAudioSource)
};

} // namespace audium
