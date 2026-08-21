//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "audium_AudioTransportSource.h"

namespace audium
{

AudioTransportSource::AudioTransportSource (std::shared_ptr<ClipDynamicsProcessor> dynamicsProcessor_) :
    dynamicsProcessor (std::move (dynamicsProcessor_))
{
    jassert (dynamicsProcessor != nullptr);
}

AudioTransportSource::~AudioTransportSource()
{
    setSource (nullptr);
    releaseMasterResources();
}

void AudioTransportSource::setSource (PositionableAudioSource* const newSource,
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

void AudioTransportSource::start()
{
    jassert(isPrepared);
    if ((! playing) && masterSource != nullptr) {
        playing = true;
        stopped = false;
        fadeOutLastBlock = false;
    }
}

void AudioTransportSource::stop(bool fadeout_)
{
    jassert(isPrepared);
    if (playing) {
        playing = false;
        stopped = true;
    }
    fadeOutLastBlock = fadeout_;
}

void AudioTransportSource::setPosition (double newPosition)
{
    jassert(isPrepared);
    if (sampleRate > 0.0)
        setNextReadPosition ((int64) (newPosition * sampleRate));
}

double AudioTransportSource::getCurrentPosition() const
{
    jassert(isPrepared);
    if (sampleRate > 0.0)
        return (double) getNextReadPosition() / sampleRate;

    return 0.0;
}

double AudioTransportSource::getLengthInSeconds() const
{
    if (sampleRate > 0.0)
        return (double) getTotalLength() / sampleRate;

    return 0.0;
}

bool AudioTransportSource::hasStreamFinished() const noexcept
{
    return positionableSource->getNextReadPosition() > positionableSource->getTotalLength() + 1
              && ! positionableSource->isLooping();
}

void AudioTransportSource::setNextReadPosition (int64 newPosition)
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

int64 AudioTransportSource::getNextReadPosition() const
{
    if (positionableSource != nullptr) {
        const double ratio = (sampleRate > 0 && sourceSampleRate > 0) ? sampleRate / sourceSampleRate : 1.0;
        return (int64) ((double) positionableSource->getNextReadPosition() * ratio);
    }

    return 0;
}

int64 AudioTransportSource::getTotalLength() const
{
    if (positionableSource != nullptr) {
        const double ratio = (sampleRate > 0 && sourceSampleRate > 0) ? sampleRate / sourceSampleRate : 1.0;
        return (int64) ((double) positionableSource->getTotalLength() * ratio);
    }
    return 0;
}

bool AudioTransportSource::isLooping() const
{
    return positionableSource != nullptr && positionableSource->isLooping();
}

void AudioTransportSource::prepareToPlay (int samplesPerBlockExpected, double newSampleRate)
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

void AudioTransportSource::releaseMasterResources()
{
    isPrepared = false;

    if (masterSource != nullptr)
        masterSource->releaseResources();

    
}

void AudioTransportSource::releaseResources()
{
    releaseMasterResources();
}

void AudioTransportSource::getNextAudioBlock (const AudioSourceChannelInfo& info)
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

void AudioTransportSource::setGain (const float newGain) noexcept
{
    dynamicsProcessor->setGain(newGain);
}

float AudioTransportSource::getGain() const noexcept
{
    return dynamicsProcessor->getGain();
}

void AudioTransportSource::resetClipGain()
{
    dynamicsProcessor->resetGain();
}

void configureClipFades (AudioTransportSource& source,
                         const ClipFadeSpec& spec,
                         double filePositionSeconds,
                         bool reset)
{
    if (spec.fadeIn <= 0.0 && spec.fadeInStart == 0.0) {
        source.clearFadeIn();
    }
    else {
        source.setFadeInCurve(spec.fadeInCurve);
        // a pre-file clamp makes filePosition > regionStart + fadeInStart,
        // which arms the ramp mid-way - the ramp progresses through the
        // pre-file silence
        source.setFadeInRamp(spec.fadeIn - spec.fadeInStart,
                             (spec.regionStart + spec.fadeInStart) - filePositionSeconds,
                             reset);
    }

    if (spec.fadeOut <= 0.0 && spec.fadeOutEnd == 0.0) {
        source.clearFadeOut();
    }
    else {
        source.setFadeOutCurve(spec.fadeOutCurve);
        source.setFadeOutRamp(spec.fadeOut - spec.fadeOutEnd,
                              (spec.regionEnd - spec.fadeOut) - filePositionSeconds,
                              reset);
    }
}

void AudioTransportSource::clearFadeIn()
{
    dynamicsProcessor->clearFadeIn();
}

void AudioTransportSource::setFadeInCurve(double curve)
{
    dynamicsProcessor->setFadeInCurve(curve);
}

void AudioTransportSource::setFadeOutCurve(double curve)
{
    dynamicsProcessor->setFadeOutCurve(curve);
}

void AudioTransportSource::clearFadeOut()
{
    dynamicsProcessor->clearFadeOut();
}

void AudioTransportSource::setFadeInRamp(double rampSeconds, double rampStartSeconds, bool reset)
{
    dynamicsProcessor->setFadeInRamp(rampSeconds, rampStartSeconds, reset);
}

void AudioTransportSource::setFadeOutRamp(double rampSeconds, double rampStartSeconds, bool reset)
{
    dynamicsProcessor->setFadeOutRamp(rampSeconds, rampStartSeconds, reset);
}

} // namespace audium
