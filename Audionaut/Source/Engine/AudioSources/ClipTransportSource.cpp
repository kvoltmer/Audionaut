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

void ClipTransportSource::setSource (PositionableAudioSource* const newSource,
                                      int readAheadSize, TimeSliceThread* readAheadThread,
                                      double sourceSampleRateToCorrectFor, int maxNumChannels)
{
    isPrepared = false;
    
    if (source == newSource)
    {
        if (source == nullptr)
            return;

        setSource (nullptr, 0, nullptr); // deselect and reselect to avoid releasing resources wrongly
    }
    

    ResamplingAudioSource* newResamplerSource = nullptr;
    BufferingAudioSource* newBufferingSource = nullptr;
    PositionableAudioSource* newPositionableSource = nullptr;
    AudioSource* newMasterSource = nullptr;

    std::unique_ptr<ResamplingAudioSource> oldResamplerSource (resamplerSource);
    std::unique_ptr<BufferingAudioSource> oldBufferingSource (bufferingSource);
    AudioSource* oldMasterSource = masterSource;

    if (newSource != nullptr)
    {
        newPositionableSource = newSource;

        if (readAheadSize > 0)
        {
            // If you want to use a read-ahead buffer, you must also provide a TimeSliceThread
            // for it to use!
            jassert (readAheadThread != nullptr);

            newPositionableSource = newBufferingSource
                = new BufferingAudioSource (newPositionableSource, *readAheadThread,
                                            false, readAheadSize, maxNumChannels);
        }

        newPositionableSource->setNextReadPosition (0);

        if (sourceSampleRateToCorrectFor > 0)
            newMasterSource = newResamplerSource
                = new ResamplingAudioSource (newPositionableSource, false, maxNumChannels);
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
        readAheadBufferSize = readAheadSize;
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
        setNextReadPosition ((int64) (newPosition * sampleRate));
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
    return positionableSource->getNextReadPosition() > positionableSource->getTotalLength() + 1
              && ! positionableSource->isLooping();
}

void ClipTransportSource::setNextReadPosition (int64 newPosition)
{
    // std::cout << "setNextReadPosition " << newPosition << std::endl;
    if (positionableSource != nullptr) {
        if (sampleRate > 0 && sourceSampleRate > 0)
            newPosition = (int64) ((double) newPosition * sourceSampleRate / sampleRate);

        positionableSource->setNextReadPosition (newPosition);

        if (resamplerSource != nullptr)
            resamplerSource->flushBuffers();
    }
}

int64 ClipTransportSource::getNextReadPosition() const
{
    if (positionableSource != nullptr) {
        const double ratio = (sampleRate > 0 && sourceSampleRate > 0) ? sampleRate / sourceSampleRate : 1.0;
        return (int64) ((double) positionableSource->getNextReadPosition() * ratio);
    }

    return 0;
}

int64 ClipTransportSource::getTotalLength() const
{
    if (positionableSource != nullptr) {
        const double ratio = (sampleRate > 0 && sourceSampleRate > 0) ? sampleRate / sourceSampleRate : 1.0;
        return (int64) ((double) positionableSource->getTotalLength() * ratio);
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
        resamplerSource->setResamplingRatio (sourceSampleRate / sampleRate);

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

void ClipTransportSource::getNextAudioBlock (const AudioSourceChannelInfo& info)
{
    if (masterSource != nullptr &&
        isPrepared)
    {
        if (playing || stopped) {
            
            masterSource->getNextAudioBlock (info);
            
            if (!playing && stopped) {
                if (fadeOutLastBlock) {
                    fadeOutLastBlock = false;
                    // just stopped playing, so fade out the last block..
                    for (int i = info.buffer->getNumChannels(); --i >= 0;)
                        info.buffer->applyGainRamp (i, info.startSample, jmin (256, info.numSamples), 1.0f, 0.0f);
                    
                    if (info.numSamples > 256)
                        info.buffer->clear (info.startSample + 256, info.numSamples - 256);
                    
                }
                stopped = true;
            }
            
            if (hasStreamFinished())
                stop(false);
            
            // clip gain and fades
            juce::dsp::AudioBlock<float> audioBlock (*info.buffer);
            juce::dsp::ProcessContextReplacing<float> gainContext(audioBlock);

            dynamicsProcessor->process(gainContext);
        }
        else {
            info.clearActiveBufferRegion();
            stopped = true;
        }
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
