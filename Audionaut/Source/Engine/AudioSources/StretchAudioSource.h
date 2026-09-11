//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <memory>
#include <JuceHeader.h>

#include "Engine/AudioSources/Stretch/StretchBackend.h"

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

    The stretching itself is done by a StretchBackend: the engine selected
    process-wide (StretchEngines::getSelected) when prepareToPlay runs, so
    an engine change takes effect when the audio device restarts or on the
    next bounce. Signalsmith Stretch ships; the other engines are there to
    be evaluated against it.

    The node lives in the chain permanently and is toggled per voice: the
    scheduler sets mode and speed before it schedules the position, and a
    position change re-primes on the next rendered block. Priming hands the
    backend look-ahead input so the very first process() output is aligned
    with the scheduled position - no latency reaches the timeline.

    Real-time notes: after prepareToPlay nothing here allocates; upstream
    input is pulled in prepared-block-size chunks so the resampler and
    buffering sources never see a larger request than they prepared for.

    @see ClipTransportSource, StretchMode, StretchBackend
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

    /// The engine this node was prepared with.
    StretchEngine getEngine() const noexcept         { return preparedEngine; }

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

    std::unique_ptr<StretchBackend> backend;
    StretchEngine preparedEngine = StretchEngine::Signalsmith;

    std::atomic<double> speedRatio { 1.0 };
    std::atomic<bool> enabled { false };
    std::atomic<bool> needsPriming { true };

    juce::AudioBuffer<float> inputScratch;
    int preparedBlockSize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StretchAudioSource)
};

} // namespace audium
