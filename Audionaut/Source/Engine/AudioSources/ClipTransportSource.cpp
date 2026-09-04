//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ClipTransportSource.h"

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
                                      int readAheadSize, juce::TimeSliceThread* readAheadThread,
                                      double sourceSampleRateToCorrectFor, int maxNumChannels)
{
    isPrepared = false;

    if (source == newSource)
    {
        if (source == nullptr)
            return;

        setSource (nullptr, 0, nullptr); // deselect and reselect to avoid releasing resources wrongly
    }

    juce::ResamplingAudioSource* newResamplerSource = nullptr;
    juce::BufferingAudioSource* newBufferingSource = nullptr;
    juce::PositionableAudioSource* newPositionableSource = nullptr;
    juce::AudioSource* newMasterSource = nullptr;

    std::unique_ptr<juce::ResamplingAudioSource> oldResamplerSource (resamplerSource);
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
            newMasterSource = newResamplerSource
                = new juce::ResamplingAudioSource (newPositionableSource, false, maxNumChannels);
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
        bufferingSource = newBufferingSource;
        masterSource = newMasterSource;
        positionableSource = newPositionableSource;
        sourceSampleRate = sourceSampleRateToCorrectFor;

        playing = false;
    }

    if (oldMasterSource != nullptr)
        oldMasterSource->releaseResources();
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
        setNextReadPosition ((juce::int64) (newPosition * sampleRate));
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
        if (sampleRate > 0 && sourceSampleRate > 0)
            newPosition = (juce::int64) ((double) newPosition * sourceSampleRate / sampleRate);

        positionableSource->setNextReadPosition (newPosition);

        if (resamplerSource != nullptr)
            resamplerSource->flushBuffers();
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

    if (masterSource != nullptr)
        masterSource->prepareToPlay (samplesPerBlockExpected, sampleRate);

    if (resamplerSource != nullptr && sourceSampleRate > 0)
        resamplerSource->setResamplingRatio (sourceSampleRate * speedRatio.load() / sampleRate);

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

    if (isPrepared && resamplerSource != nullptr && sourceSampleRate > 0 && sampleRate > 0)
        resamplerSource->setResamplingRatio (sourceSampleRate * newSpeedRatio / sampleRate);
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
