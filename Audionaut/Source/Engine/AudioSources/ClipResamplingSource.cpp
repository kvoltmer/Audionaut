//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ClipResamplingSource.h"

namespace audium
{

ClipResamplingSource::ClipResamplingSource (juce::AudioSource* const inputSource,
                                            const bool deleteInputWhenDeleted,
                                            const int channels)
    : input (inputSource, deleteInputWhenDeleted),
      numChannels (channels)
{
    jassert (input != nullptr);
    juce::zeromem (coefficients, sizeof (coefficients));
}

ClipResamplingSource::~ClipResamplingSource() {}

void ClipResamplingSource::setResamplingRatio (const double samplesInPerOutputSample)
{
    jassert (samplesInPerOutputSample > 0);

    const juce::SpinLock::ScopedLockType sl (ratioLock);
    ratio = juce::jmax (0.0, samplesInPerOutputSample);
}

void ClipResamplingSource::setMaximumResamplingRatio (const double maximumSamplesInPerOutputSample)
{
    jassert (maximumSamplesInPerOutputSample > 0);

    const juce::SpinLock::ScopedLockType sl (ratioLock);
    maximumRatio = juce::jmax (0.0, maximumSamplesInPerOutputSample);
}

void ClipResamplingSource::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    const juce::SpinLock::ScopedLockType sl (ratioLock);

    auto scaledBlockSize = juce::roundToInt (samplesPerBlockExpected * ratio);
    input->prepareToPlay (scaledBlockSize, sampleRate * ratio);

    // Sized for the largest ratio the clip may reach, not the current one:
    // getNextAudioBlock needs roundToInt (numSamples * ratio) + 11 and
    // would otherwise grow the buffer on the audio thread.
    const auto maxScaledBlockSize = juce::roundToInt (samplesPerBlockExpected * juce::jmax (ratio, maximumRatio));
    buffer.setSize (numChannels, maxScaledBlockSize + 32);
    bufferGrowths = 0;

    filterStates.calloc (numChannels);
    srcBuffers.calloc (numChannels);
    destBuffers.calloc (numChannels);
    createLowPass (ratio);

    flushBuffers();
}

void ClipResamplingSource::flushBuffers()
{
    const juce::ScopedLock sl (callbackLock);

    buffer.clear();
    bufferPos = 0;
    sampsInBuffer = 0;
    subSampleOffset = 0.0;
    resetFilters();
}

void ClipResamplingSource::releaseResources()
{
    input->releaseResources();
    buffer.setSize (numChannels, 0);
}

void ClipResamplingSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    const juce::ScopedLock sl (callbackLock);

    double localRatio;

    {
        const juce::SpinLock::ScopedLockType ratioSl (ratioLock);
        localRatio = ratio;
    }

    if (! juce::approximatelyEqual (lastRatio, localRatio))
    {
        createLowPass (localRatio);
        lastRatio = localRatio;
    }

    const int sampsNeeded = juce::roundToInt (info.numSamples * localRatio) + 3;

    int bufferSize = buffer.getNumSamples();

    if (bufferSize < sampsNeeded + 8)
    {
        // The fallback prepareToPlay is meant to make unreachable: only a
        // ratio beyond the maximum or a block beyond the prepared size
        // gets here, and it allocates on the audio thread.
        ++bufferGrowths;
        DBG ("ClipResamplingSource: buffer grown in getNextAudioBlock (ratio " << localRatio
             << ", max " << maximumRatio << ", block " << info.numSamples << ")");

        bufferPos %= bufferSize;
        bufferSize = sampsNeeded + 32;
        buffer.setSize (buffer.getNumChannels(), bufferSize, true, true);
    }

    bufferPos %= bufferSize;

    int endOfBufferPos = bufferPos + sampsInBuffer;
    const int channelsToProcess = juce::jmin (numChannels, info.buffer->getNumChannels());

    while (sampsNeeded > sampsInBuffer)
    {
        endOfBufferPos %= bufferSize;

        int numToDo = juce::jmin (sampsNeeded - sampsInBuffer,
                                  bufferSize - endOfBufferPos);

        juce::AudioSourceChannelInfo readInfo (&buffer, endOfBufferPos, numToDo);
        input->getNextAudioBlock (readInfo);

        if (localRatio > 1.0001)
        {
            // for down-sampling, pre-apply the filter

            for (int i = channelsToProcess; --i >= 0;)
                applyFilter (buffer.getWritePointer (i, endOfBufferPos), numToDo, filterStates[i]);
        }

        sampsInBuffer += numToDo;
        endOfBufferPos += numToDo;
    }

    for (int channel = 0; channel < channelsToProcess; ++channel)
    {
        destBuffers[channel] = info.buffer->getWritePointer (channel, info.startSample);
        srcBuffers[channel] = buffer.getReadPointer (channel);
    }

    int nextPos = (bufferPos + 1) % bufferSize;

    for (int m = info.numSamples; --m >= 0;)
    {
        jassert (sampsInBuffer > 0 && nextPos != endOfBufferPos);

        const float alpha = (float) subSampleOffset;

        for (int channel = 0; channel < channelsToProcess; ++channel)
            *destBuffers[channel]++ = srcBuffers[channel][bufferPos]
                                        + alpha * (srcBuffers[channel][nextPos] - srcBuffers[channel][bufferPos]);

        subSampleOffset += localRatio;

        while (subSampleOffset >= 1.0)
        {
            if (++bufferPos >= bufferSize)
                bufferPos = 0;

            --sampsInBuffer;

            nextPos = (bufferPos + 1) % bufferSize;
            subSampleOffset -= 1.0;
        }
    }

    if (localRatio < 0.9999)
    {
        // for up-sampling, apply the filter after transposing
        for (int i = channelsToProcess; --i >= 0;)
            applyFilter (info.buffer->getWritePointer (i, info.startSample), info.numSamples, filterStates[i]);
    }
    else if (localRatio <= 1.0001 && info.numSamples > 0)
    {
        // if the filter's not currently being applied, keep it stoked with the last couple of samples to avoid discontinuities
        for (int i = channelsToProcess; --i >= 0;)
        {
            const float* const endOfBuffer = info.buffer->getReadPointer (i, info.startSample + info.numSamples - 1);
            FilterState& fs = filterStates[i];

            if (info.numSamples > 1)
            {
                fs.y2 = fs.x2 = *(endOfBuffer - 1);
            }
            else
            {
                fs.y2 = fs.y1;
                fs.x2 = fs.x1;
            }

            fs.y1 = fs.x1 = *endOfBuffer;
        }
    }

    jassert (sampsInBuffer >= 0);
}

void ClipResamplingSource::createLowPass (const double frequencyRatio)
{
    const double proportionalRate = (frequencyRatio > 1.0) ? 0.5 / frequencyRatio
                                                           : 0.5 * frequencyRatio;

    const double n = 1.0 / std::tan (juce::MathConstants<double>::pi * juce::jmax (0.001, proportionalRate));
    const double nSquared = n * n;
    const double c1 = 1.0 / (1.0 + juce::MathConstants<double>::sqrt2 * n + nSquared);

    setFilterCoefficients (c1,
                           c1 * 2.0f,
                           c1,
                           1.0,
                           c1 * 2.0 * (1.0 - nSquared),
                           c1 * (1.0 - juce::MathConstants<double>::sqrt2 * n + nSquared));
}

void ClipResamplingSource::setFilterCoefficients (double c1, double c2, double c3, double c4, double c5, double c6)
{
    const double a = 1.0 / c4;

    c1 *= a;
    c2 *= a;
    c3 *= a;
    c5 *= a;
    c6 *= a;

    coefficients[0] = c1;
    coefficients[1] = c2;
    coefficients[2] = c3;
    coefficients[3] = c4;
    coefficients[4] = c5;
    coefficients[5] = c6;
}

void ClipResamplingSource::resetFilters()
{
    if (filterStates != nullptr)
        filterStates.clear ((size_t) numChannels);
}

void ClipResamplingSource::applyFilter (float* samples, int num, FilterState& fs)
{
    while (--num >= 0)
    {
        const double in = *samples;

        double out = coefficients[0] * in
                     + coefficients[1] * fs.x1
                     + coefficients[2] * fs.x2
                     - coefficients[4] * fs.y1
                     - coefficients[5] * fs.y2;

       #if JUCE_INTEL
        if (! (out < -1.0e-8 || out > 1.0e-8))
            out = 0;
       #endif

        fs.x2 = fs.x1;
        fs.x1 = in;
        fs.y2 = fs.y1;
        fs.y1 = out;

        *samples++ = (float) out;
    }
}

} // namespace audium
