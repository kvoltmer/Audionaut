//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <cmath>
#include "ClipTransportSource.h"
#include "Engine/AudioSources/RenderTiming.h"
#include "Engine/PlayList/ClipSpeed.h"

namespace audium
{

ClipTransportSource::ClipTransportSource (std::shared_ptr<ClipDynamicsProcessor> dynamicsProcessor_) :
    dynamicsProcessor (std::move (dynamicsProcessor_))
{
    jassert (dynamicsProcessor != nullptr);
}

ClipTransportSource::~ClipTransportSource()
{
    setSource (nullptr);
    releaseMasterResources();
}

void ClipTransportSource::setSource (juce::PositionableAudioSource* const newSource,
                                      int newReadAheadSize, juce::TimeSliceThread* newReadAheadThread,
                                      double sourceSampleRateToCorrectFor, int newMaxNumChannels)
{
    isPrepared = false;

    if (source == newSource)
    {
        if (source == nullptr)
            return;

        setSource (nullptr, 0, nullptr); // deselect and reselect to avoid releasing resources wrongly
    }

    ClipResamplingSource* newResamplerSource = nullptr;
    StretchAudioSource* newStretchSource = nullptr;
    juce::BufferingAudioSource* newBufferingSource = nullptr;
    juce::PositionableAudioSource* newPositionableSource = nullptr;
    juce::AudioSource* newMasterSource = nullptr;

    std::unique_ptr<ClipResamplingSource> oldResamplerSource (resamplerSource);
    std::unique_ptr<ClipResamplingSource> oldStandbyResampler (standbyResampler);
    std::unique_ptr<StretchAudioSource> oldStretchSource (stretchSource);
    // the buffer's destructor releases it, which unregisters it from the
    // read-ahead thread before the cursor behind it can go
    std::unique_ptr<juce::BufferingAudioSource> oldStandbyBuffering (standbyBuffering);
    standbyResampler = nullptr;
    standbyBuffering = nullptr;
    standbySource = nullptr;
    standbyInput = nullptr;
    standbyKey = -1;
    maxNumChannels = newMaxNumChannels;
    readAheadSize = newSource != nullptr ? newReadAheadSize : 0;
    readAheadThread = newSource != nullptr ? newReadAheadThread : nullptr;
    std::unique_ptr<juce::BufferingAudioSource> oldBufferingSource (bufferingSource);
    juce::AudioSource* oldMasterSource = masterSource;

    if (newSource != nullptr)
    {
        newPositionableSource = newSource;

        if (readAheadSize > 0)
        {
            // If you want to use a read-ahead buffer, you must also provide a TimeSliceThread
            // for it to use!
            jassert (readAheadThread != nullptr);

            newPositionableSource = newBufferingSource
                = new juce::BufferingAudioSource (newPositionableSource, *readAheadThread,
                                                  false, readAheadSize, maxNumChannels);
        }

        newPositionableSource->setNextReadPosition (0);

        if (sourceSampleRateToCorrectFor > 0)
        {
            newMasterSource = newResamplerSource
                = new ClipResamplingSource (newPositionableSource, false, maxNumChannels);

            // the pitch-preserving node lives in the chain permanently and
            // is bypassed in RePitch mode - see setStretchMode
            newMasterSource = newStretchSource
                = new StretchAudioSource (newResamplerSource, maxNumChannels);

            // The stretcher primes with a large look-ahead right after a
            // position jump, which the read-ahead thread has not buffered
            // yet; reading it anyway consumes silence in place of the clip's
            // start. So the node asks first (in its own, device-rate domain)
            // and defers the prime while the buffer catches up. Offline the
            // probe waits, live it gives the reader a moment at most. The
            // probe reads the member: a lane swap makes another buffer the
            // live one.
            if (newBufferingSource != nullptr)
            {
                const auto sourceRate = sourceSampleRateToCorrectFor;
                const auto readAhead = readAheadSize;

                newStretchSource->setInputReadiness (
                    [this, sourceRate] (int numDeviceSamples)
                    {
                        const auto sourceSamples = static_cast<int> (std::ceil (numDeviceSamples * sourceRate / sampleRate)) + 64;
                        juce::AudioSourceChannelInfo probe (nullptr, 0, sourceSamples);
                        return bufferingSource->waitForNextAudioBlockReady (probe, RenderTiming::inputReadinessTimeoutMs());
                    },
                    [this, sourceRate, readAhead]
                    {
                        // what the read-ahead can hold at all, in device samples
                        return static_cast<int> ((readAhead - 4096) * sampleRate / sourceRate);
                    });
            }
        }
        else
            newMasterSource = newPositionableSource;

        if (isPrepared)
        {
            if (newResamplerSource != nullptr && sourceSampleRateToCorrectFor > 0 && sampleRate > 0)
                newResamplerSource->setResamplingRatio (sourceSampleRateToCorrectFor / sampleRate);

            newMasterSource->prepareToPlay (blockSize, sampleRate);
        }
    }

    {
        source = newSource;
        resamplerSource = newResamplerSource;
        stretchSource = newStretchSource;
        bufferingSource = newBufferingSource;
        masterSource = newMasterSource;
        positionableSource = newPositionableSource;
        sourceSampleRate = sourceSampleRateToCorrectFor;

        playing = false;
    }

    if (oldMasterSource != nullptr)
        oldMasterSource->releaseResources();
}

void ClipTransportSource::setStandbySource (juce::PositionableAudioSource* newStandbySource)
{
    std::unique_ptr<ClipResamplingSource> oldStandbyResampler (standbyResampler);
    std::unique_ptr<juce::BufferingAudioSource> oldStandbyBuffering (standbyBuffering);
    standbyResampler = nullptr;
    standbyBuffering = nullptr;
    standbySource = nullptr;
    standbyInput = nullptr;
    standbyKey = -1;

    if (stretchSource != nullptr)
        stretchSource->setStandbyInput (nullptr);

    if (oldStandbyResampler != nullptr)
        oldStandbyResampler->releaseResources();

    if (newStandbySource == nullptr || stretchSource == nullptr)
        return;

    newStandbySource->setNextReadPosition (0);
    standbyInput = newStandbySource;
    standbySource = newStandbySource;

    if (bufferingSource != nullptr)
    {
        // a buffered chain reads ahead on the background thread, and a
        // position jump empties that buffer: the lane gets a read-ahead of
        // its own, which primeStandby seeks to the upcoming position so
        // the fill runs over the blocks before the jump. No second
        // stretcher here - the stretch node keeps its in-block prime, which
        // then finds its input ready instead of deferring
        standbySource = standbyBuffering
            = new juce::BufferingAudioSource (standbyInput, *readAheadThread, false, readAheadSize, maxNumChannels);
        standbyResampler = new ClipResamplingSource (standbySource, false, maxNumChannels);

        // its bounds go in before it is prepared, like the live lane's
        boundResamplers();

        if (isPrepared)
            standbyResampler->prepareToPlay (blockSize, sampleRate);

        return;
    }

    standbyResampler = new ClipResamplingSource (standbySource, false, maxNumChannels);

    // prepares the lane too when the chain already is, so its bounds go
    // in first
    boundResamplers();
    stretchSource->setStandbyInput (standbyResampler);
}

juce::int64 ClipTransportSource::toSourceSamples (juce::int64 deviceSamples) const noexcept
{
    if (sampleRate > 0 && sourceSampleRate > 0)
        return (juce::int64) std::llround ((double) deviceSamples * sourceSampleRate / sampleRate);

    return deviceSamples;
}

void ClipTransportSource::primeStandby (double newPositionSeconds, double ratio, int blocksLeft)
{
    if (! isPrepared || stretchSource == nullptr || standbySource == nullptr
        || standbyResampler == nullptr || sampleRate <= 0.0)
        return;

    // the same rounding setPosition applies, so the keys meet at the wrap
    const auto key = toSourceSamples ((juce::int64) std::llround (newPositionSeconds * sampleRate));

    if (standbyBuffering != nullptr)
    {
        // a buffered lane: seeking it starts the read-ahead fill on the
        // background thread, and there is nothing to slice - the blocks
        // until the jump are the fill's time
        if (standbyKey != key)
        {
            standbyKey = key;
            standbySource->setNextReadPosition (key);
            standbyResampler->setResamplingRatio (currentResamplingRatio());
            standbyResampler->flushBuffers();
        }
        return;
    }

    if (! stretchSource->isStandbyPrimingFor (key))
    {
        // a fresh prime: the lane's cursor goes to the target, its
        // resampler only corrects the file's rate (Stretch mode)
        standbySource->setNextReadPosition (key);
        standbyResampler->setResamplingRatio (sourceSampleRate > 0 ? sourceSampleRate / sampleRate : 1.0);
        standbyResampler->flushBuffers();
    }

    stretchSource->primeStandby (key, ratio, blocksLeft);
}

void ClipTransportSource::start()
{
    jassert(isPrepared);
    if ((! playing) && masterSource != nullptr) {
        playing = true;
        stopped = false;
        fadeOutLastBlock = false;
    }
}

void ClipTransportSource::stop(bool fadeout_)
{
    jassert(isPrepared);
    if (playing) {
        playing = false;
        stopped = true;
    }
    fadeOutLastBlock = fadeout_;
}

void ClipTransportSource::setPosition (double newPosition)
{
    jassert(isPrepared);
    if (sampleRate > 0.0)
        setNextReadPosition ((juce::int64) std::llround (newPosition * sampleRate));
}

double ClipTransportSource::getCurrentPosition() const
{
    jassert(isPrepared);
    if (sampleRate > 0.0)
        return (double) getNextReadPosition() / sampleRate;

    return 0.0;
}

double ClipTransportSource::getLengthInSeconds() const
{
    if (sampleRate > 0.0)
        return (double) getTotalLength() / sampleRate;

    return 0.0;
}

bool ClipTransportSource::hasStreamFinished() const noexcept
{
    if (positionableSource == nullptr)
        return true;

    return positionableSource->getNextReadPosition() > positionableSource->getTotalLength() + 1
              && ! positionableSource->isLooping();
}

void ClipTransportSource::setNextReadPosition (juce::int64 newPosition)
{
    if (positionableSource != nullptr) {
        newPosition = toSourceSamples (newPosition);

        // a standby lane primed for exactly this jump takes over: its
        // cursor already sits past the look-ahead, so nothing is seeked
        // and no prime runs in this block
        if (stretchSource != nullptr
            && stretchMode.load() == StretchMode::Stretch
            && stretchSource->adoptStandby (newPosition))
        {
            std::swap (positionableSource, standbySource);
            std::swap (resamplerSource, standbyResampler);
            source = positionableSource;
            return;
        }

        // a buffered lane seeked to exactly this jump ahead of time takes
        // over: its read-ahead holds the material, so no buffer runs empty
        // in this block and nothing waits for the reader. The stretch node
        // still primes in Stretch mode - from the ready lane
        if (standbyBuffering != nullptr && standbyKey == newPosition)
        {
            std::swap (positionableSource, standbySource);
            std::swap (bufferingSource, standbyBuffering);
            std::swap (resamplerSource, standbyResampler);
            std::swap (source, standbyInput);
            standbyKey = -1;
            ++bufferedStandbyAdoptions;

            stretchSource->setInput (resamplerSource);
            updateSpeedChain();
            stretchSource->flushBuffers();
            return;
        }

        positionableSource->setNextReadPosition (newPosition);

        if (resamplerSource != nullptr)
            resamplerSource->flushBuffers();

        if (stretchSource != nullptr)
            stretchSource->flushBuffers();
    }
}

juce::int64 ClipTransportSource::getNextReadPosition() const
{
    if (positionableSource != nullptr) {
        const double ratio = (sampleRate > 0 && sourceSampleRate > 0) ? sampleRate / sourceSampleRate : 1.0;
        return (juce::int64) ((double) positionableSource->getNextReadPosition() * ratio);
    }

    return 0;
}

juce::int64 ClipTransportSource::getTotalLength() const
{
    if (positionableSource != nullptr) {
        const double ratio = (sampleRate > 0 && sourceSampleRate > 0) ? sampleRate / sourceSampleRate : 1.0;
        return (juce::int64) ((double) positionableSource->getTotalLength() * ratio);
    }
    return 0;
}

bool ClipTransportSource::isLooping() const
{
    return positionableSource != nullptr && positionableSource->isLooping();
}

void ClipTransportSource::prepareToPlay (int samplesPerBlockExpected, double newSampleRate)
{
    isPrepared = false;

    sampleRate = newSampleRate;
    blockSize = samplesPerBlockExpected;

    // The resamplers size their buffers in prepareToPlay, so they learn
    // the ratio they start at and the largest one they may reach before
    // the chain is prepared - applied afterwards (as the stock transport
    // does) the first block would have to grow them on the audio thread.
    boundResamplers();
    updateSpeedChain();

    if (masterSource != nullptr)
        masterSource->prepareToPlay (samplesPerBlockExpected, sampleRate);

    // the buffered standby lane hangs off no node of the chain
    if (standbyBuffering != nullptr)
        standbyResampler->prepareToPlay (samplesPerBlockExpected, sampleRate);

    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize    = samplesPerBlockExpected;
    spec.sampleRate          = newSampleRate;
    dynamicsProcessor->prepare(spec);
    
    isPrepared = true;
}

void ClipTransportSource::releaseMasterResources()
{
    isPrepared = false;

    if (masterSource != nullptr)
        masterSource->releaseResources();

    if (standbyBuffering != nullptr)
        standbyResampler->releaseResources();
}

void ClipTransportSource::releaseResources()
{
    releaseMasterResources();
}

void ClipTransportSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    if (masterSource == nullptr || ! isPrepared)
        return;

    if (! playing && ! stopped) {
        // transient window while stop() flips the two flags
        info.clearActiveBufferRegion();
        stopped = true;
        return;
    }

    masterSource->getNextAudioBlock (info);

    if (!playing && stopped) {
        if (fadeOutLastBlock) {
            fadeOutLastBlock = false;
            // just stopped playing, so fade out the last block..
            for (int i = info.buffer->getNumChannels(); --i >= 0;)
                info.buffer->applyGainRamp (i, info.startSample, juce::jmin (256, info.numSamples), 1.0f, 0.0f);

            if (info.numSamples > 256)
                info.buffer->clear (info.startSample + 256, info.numSamples - 256);
        }
        stopped = true;
    }

    if (hasStreamFinished())
        stop(false);

    // clip gain and fades - only over the active region: callers split a
    // callback into sub-blocks (loop wrap, clip end), and the fade ramps
    // are sample-counter-stateful
    juce::dsp::AudioBlock<float> audioBlock (*info.buffer);
    auto activeBlock = audioBlock.getSubBlock ((size_t) info.startSample,
                                               (size_t) info.numSamples);
    juce::dsp::ProcessContextReplacing<float> gainContext(activeBlock);

    dynamicsProcessor->process(gainContext);
}

void ClipTransportSource::setSpeedRatio (double newSpeedRatio) noexcept
{
    jassert (newSpeedRatio > 0.0);
    speedRatio.store (newSpeedRatio);

    if (isPrepared)
        updateSpeedChain();
}

void ClipTransportSource::setStretchMode (StretchMode newMode) noexcept
{
    stretchMode.store (newMode);

    if (isPrepared)
        updateSpeedChain();
}

double ClipTransportSource::currentResamplingRatio() const noexcept
{
    // In Stretch mode the resampler only corrects the file's sample rate;
    // the stretch node realises the speed and keeps the pitch.
    const bool stretching = stretchMode.load() == StretchMode::Stretch;

    if (sourceSampleRate > 0 && sampleRate > 0)
        return sourceSampleRate * (stretching ? 1.0 : speedRatio.load()) / sampleRate;

    return 1.0;
}

void ClipTransportSource::updateSpeedChain() noexcept
{
    const auto speed = speedRatio.load();
    const bool stretching = stretchMode.load() == StretchMode::Stretch;

    if (resamplerSource != nullptr && sourceSampleRate > 0 && sampleRate > 0)
        resamplerSource->setResamplingRatio (currentResamplingRatio());

    if (stretchSource != nullptr)
    {
        stretchSource->setEnabled (stretching);
        stretchSource->setSpeedRatio (speed);
    }
}

void ClipTransportSource::boundResamplers() noexcept
{
    if (sourceSampleRate <= 0 || sampleRate <= 0)
        return;

    // Either lane can become the live one (the standby swap in
    // setNextReadPosition), so both are sized for the fastest varispeed
    // the clip can reach on top of the file's rate correction. At the
    // extremes (192 kHz file at 44.1 kHz, speed 4) that is 17.4 input
    // samples per output sample: a 2048 block then holds 36k samples per
    // channel, 128 blocks 2.2k - a fraction of what the stretcher keeps.
    const auto rateCorrection = sourceSampleRate / sampleRate;
    const auto maximumRatio = rateCorrection * ClipSpeed::maxSpeedRatio;

    if (resamplerSource != nullptr)
        resamplerSource->setMaximumResamplingRatio (maximumRatio);

    if (standbyResampler != nullptr)
    {
        standbyResampler->setMaximumResamplingRatio (maximumRatio);
        // an unbuffered lane only ever corrects the rate (it primes Stretch
        // mode, see primeStandby); a buffered one plays on in either mode
        standbyResampler->setResamplingRatio (standbyBuffering != nullptr ? currentResamplingRatio() : rateCorrection);
    }
}

void ClipTransportSource::setGain (const float newGain) noexcept
{
    dynamicsProcessor->setGain(newGain);
}

float ClipTransportSource::getGain() const noexcept
{
    return dynamicsProcessor->getGain();
}

void ClipTransportSource::resetClipGain()
{
    dynamicsProcessor->resetGain();
}

void ClipTransportSource::clearFadeIn()
{
    dynamicsProcessor->clearFadeIn();
}

void ClipTransportSource::setFadeInCurve(double curve)
{
    dynamicsProcessor->setFadeInCurve(curve);
}

void ClipTransportSource::setFadeOutCurve(double curve)
{
    dynamicsProcessor->setFadeOutCurve(curve);
}

void ClipTransportSource::clearFadeOut()
{
    dynamicsProcessor->clearFadeOut();
}

void ClipTransportSource::setFadeInRamp(double rampSeconds, double rampStartSeconds, bool reset)
{
    dynamicsProcessor->setFadeInRamp(rampSeconds, rampStartSeconds, reset);
}

void ClipTransportSource::setFadeOutRamp(double rampSeconds, double rampStartSeconds, bool reset)
{
    dynamicsProcessor->setFadeOutRamp(rampSeconds, rampStartSeconds, reset);
}

} // namespace audium
