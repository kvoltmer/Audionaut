//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "audium_AudioTransportSource.h"

namespace audium
{

AudioTransportSource::AudioTransportSource()
{
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
    clipGain.prepare(spec);
    clipGain.setRampDurationSeconds(0.01);
    
    clipFadeIn.prepare(spec);
    clipFadeIn.setRampDurationSeconds(0.0);
    
    clipFadeOut.prepare(spec);
    clipFadeOut.setRampDurationSeconds(0.0);
    
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
            
            // clip gain
            clipGain.setGainLinear(gain.load());
            juce::dsp::AudioBlock<float> audioBlock (*info.buffer);
            juce::dsp::ProcessContextReplacing<float> gainContext(audioBlock);
            
            clipGain.process(gainContext);
            clipFadeIn.process(gainContext);
            clipFadeOut.process(gainContext);
        }
        else {
            info.clearActiveBufferRegion();
            stopped = true;
        }
    }
}

void AudioTransportSource::setGain (const float newGain) noexcept
{
    gain = newGain;
}

float AudioTransportSource::getGain() const noexcept
{
    return gain.load();
}

void AudioTransportSource::resetClipGain()
{
    auto rampDuration = clipGain.getRampDurationSeconds();
    clipGain.setRampDurationSeconds(0.0);
    clipGain.setGainLinear(gain.load());
    clipGain.setRampDurationSeconds(rampDuration);
}

void AudioTransportSource::setFadeInSeconds(double fadeInSeconds, double offsetInSeconds, bool reset)
{
    auto fadeTime = fadeInSeconds;
    auto initialGain = 0.0;
    if (offsetInSeconds < 0.0) {
        fadeTime = fadeInSeconds + offsetInSeconds;
        if (fadeTime < 0.0)
            fadeTime = 0.0;
        
        if (fadeInSeconds > 0.0)
            initialGain = 1.0 - (fadeTime / fadeInSeconds);
    }
    
    if (reset) {
        clipFadeIn.setGainLinear(initialGain, true);
    }
    clipFadeIn.setRampDurationSeconds(fadeTime);
    clipFadeIn.setGainLinear(1.0, false);
}

void AudioTransportSource::setFadeOutSeconds(double fadeOutSeconds, double duration, bool reset)
{
    if (duration < 0.0)
        duration = 0.0;
    
    auto fadeTime = fadeOutSeconds;
    auto initialGain = 1.0;
    if (fadeOutSeconds <= duration) {
        // schedule
        auto start = duration - fadeOutSeconds;
        clipFadeOut.setSkipSamples(start * sampleRate);
    }
    else {
        fadeTime = duration;
        
        if (fadeOutSeconds > 0.0)
            initialGain = duration / fadeOutSeconds;
    }
    
    if (reset)
        clipFadeOut.setGainLinear(initialGain, true);
    
    // std::cout << initialGain << " time: " << fadeTime << std::endl;
    clipFadeOut.setRampDurationSeconds(fadeTime);
    clipFadeOut.setGainLinear(0.0, false);
}

} // namespace audium
