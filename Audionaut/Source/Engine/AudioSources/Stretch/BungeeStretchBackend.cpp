//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "BungeeStretchBackend.h"

#if STRETCH_BUNGEE_ENABLED

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <bungee/Bungee.h>

namespace audium {

namespace {

/**
 * Bungee::Stream's forward-playback driver (bungee/Stream.h, MPL-2.0),
 * carried here with one addition: reset(), which the library's class lacks
 * - a voice re-primes on every position change and must not reallocate.
 */
class BungeeStream
{
public:
    using Stretcher = Bungee::Stretcher<Bungee::Basic>;

    BungeeStream (Stretcher& stretcher_, int maxInputFrameCount, int channelCount_) :
        stretcher (stretcher_),
        channelCount (channelCount_),
        channelStride (stretcher_.maxInputFrameCount() + maxInputFrameCount),
        buffer (static_cast<size_t> (channelStride * channelCount_))
    {
        reset();
    }

    void reset()
    {
        // The stretcher insists on specify -> analyse -> synthesise in strict
        // order; a grain specified by the last process() call is still
        // pending, so run it to completion (its output is dropped) before
        // starting over.
        if (grainPending)
        {
            analyseGrain();
            stretcher.synthesiseGrain (outputChunk);
            grainPending = false;
        }

        begin = end = 0;
        inputChunk = {};
        request = {};
        request.position = std::numeric_limits<double>::quiet_NaN();
        request.pitch = 1.0;
        request.speed = 1.0;
        outputChunk = {};
        outputChunkConsumed = 0;
        framesNeeded = 0.0;
    }

    /// Renders floor/ceil(outputFrameCount) samples from inputFrameCount
    /// new input frames; returns the count rendered.
    int process (const float* const* inputPointers, float* const* outputPointers,
                 int inputFrameCount, double outputFrameCount)
    {
        append (inputFrameCount, inputPointers);
        request.speed = inputFrameCount / outputFrameCount;
        request.pitch = 1.0;
        framesNeeded += outputFrameCount;

        int frameCounter = 0;
        for (bool processGrain = false; frameCounter != static_cast<int> (std::round (framesNeeded)); processGrain = true)
        {
            if (processGrain)
            {
                if (! std::isnan (request.position))
                {
                    analyseGrain();
                    stretcher.synthesiseGrain (outputChunk);
                    grainPending = false;
                    outputChunkConsumed = 0;
                }

                const double denominator = std::round (outputFrameCount);
                const double numerator = denominator - frameCounter;
                const auto position = end - stretcher.maxInputFrameCount() / 2
                                      - inputFrameCount * numerator / denominator;
                request.reset = ! (position > request.position);
                request.position = position;
                inputChunk = stretcher.specifyGrain (request);
                grainPending = true;
            }

            if (outputChunk.request[0] != nullptr && ! std::isnan (outputChunk.request[0]->position))
            {
                const int need = static_cast<int> (std::round (framesNeeded)) - frameCounter;
                const int available = outputChunk.frameCount - outputChunkConsumed;
                const int n = std::min (need, available);

                for (int c = 0; c < channelCount; ++c)
                    std::copy (outputChunk.data + outputChunkConsumed + c * outputChunk.channelStride,
                               outputChunk.data + outputChunkConsumed + c * outputChunk.channelStride + n,
                               outputPointers[c] + frameCounter);

                frameCounter += n;
                outputChunkConsumed += n;
            }
        }

        framesNeeded -= frameCounter;
        return frameCounter;
    }

    /// Input position (in input frames, from the stream start) of the next
    /// output sample process() would deliver; NaN before the first grain.
    double outputPosition() const
    {
        if (outputChunk.request[0] == nullptr || outputChunk.request[1] == nullptr || outputChunk.frameCount <= 0)
            return std::numeric_limits<double>::quiet_NaN();

        return outputChunk.request[0]->position
               + outputChunkConsumed * (outputChunk.request[1]->position - outputChunk.request[0]->position)
                     / outputChunk.frameCount;
    }

private:
    void append (int inputFrameCount, const float* const* inputPointers)
    {
        int discard = 0;
        if (inputChunk.begin < end)
        {
            if (begin < inputChunk.begin)
            {
                for (int x = 0; x < static_cast<int> (buffer.size()); x += channelStride)
                    std::move (&buffer[static_cast<size_t> (x + inputChunk.begin - begin)],
                               &buffer[static_cast<size_t> (x + end - begin)],
                               &buffer[static_cast<size_t> (x)]);
                begin = inputChunk.begin;
            }
        }
        else
        {
            discard = std::min (inputChunk.begin - begin, inputFrameCount);
            begin = end;
        }

        for (int c = 0; c < channelCount; ++c)
        {
            auto* dest = &buffer[static_cast<size_t> ((end - begin) + c * channelStride)];
            if (inputPointers != nullptr)
                std::copy (&inputPointers[c][discard], &inputPointers[c][inputFrameCount], dest);
            else
                std::fill (dest, dest + (inputFrameCount - discard), 0.0f);
        }

        begin += discard;
        end += inputFrameCount;
    }

    void analyseGrain()
    {
        const int muteHead = begin - inputChunk.begin;
        const int muteTail = inputChunk.end - end;
        stretcher.analyseGrain (buffer.data() - muteHead, channelStride, muteHead, muteTail);
    }

    Stretcher& stretcher;
    const int channelCount;
    const int channelStride;
    std::vector<float> buffer;
    int begin = 0, end = 0;
    Bungee::InputChunk inputChunk {};
    Bungee::Request request {};
    Bungee::OutputChunk outputChunk {};
    int outputChunkConsumed = 0;
    double framesNeeded = 0.0;
    bool grainPending = false;
};

} // namespace

struct BungeeStretchBackend::Impl
{
    std::unique_ptr<Bungee::Stretcher<Bungee::Basic>> stretcher;
    std::unique_ptr<BungeeStream> stream;
};

BungeeStretchBackend::BungeeStretchBackend() : impl (std::make_unique<Impl>()) {}
BungeeStretchBackend::~BungeeStretchBackend() = default;

void BungeeStretchBackend::prepare (int numChannels_, double sampleRate, int maxBlockSize, double maxSpeedRatio)
{
    numChannels = numChannels_;
    headroom = maxBlockSize;
    preparedBlockSize = maxBlockSize;
    maxInputPerCall = maxInputLength (maxBlockSize, maxSpeedRatio);
    calibratedRatio = 0.0;

    const auto rate = static_cast<int> (std::lround (sampleRate));
    impl->stream.reset();
    impl->stretcher = std::make_unique<Bungee::Stretcher<Bungee::Basic>> (Bungee::SampleRates { rate, rate }, numChannels);
    windowFrames = impl->stretcher->maxInputFrameCount();
    impl->stream = std::make_unique<BungeeStream> (*impl->stretcher, maxInputPerCall, numChannels);

    // the largest render: the priming read (or the alignment pad, up to two
    // windows of zeros) at the slowest speed (1/max)
    const auto minRatio = 1.0 / maxSpeedRatio;
    const auto maxPad = 2 * windowFrames + 4096;
    zeros.setSize (numChannels, maxPad);
    zeros.clear();
    const auto primeRender = static_cast<int> (std::ceil (maxInputPerCall / minRatio)) + 2;
    const auto padRender = static_cast<int> (std::ceil (maxPad / minRatio)) + 2;
    const auto blockRender = static_cast<int> (std::ceil (maxInputPerCall / minRatio)) + 2;
    renderScratch.setSize (numChannels, juce::jmax (primeRender, juce::jmax (padRender, blockRender)));

    fifo.prepare (numChannels, juce::jmax (4 * renderScratch.getNumSamples(), 16 * maxBlockSize));

    // The calibrator is a second raw pipeline (it gets no calibrator of its
    // own); the step signal covers a prime at the fastest speed plus a
    // window and a margin.
    if (calibrator == nullptr && ! isCalibratorInstance)
    {
        calibrator = std::make_unique<BungeeStretchBackend>();
        calibrator->isCalibratorInstance = true;
        calibrator->prepare (numChannels, sampleRate, maxBlockSize, maxSpeedRatio);
        stepScratch.setSize (numChannels, 4 * windowFrames + 16384);
        calibrationOut.setSize (numChannels, maxBlockSize);
    }
}

int BungeeStretchBackend::maxInputLength (int maxBlockSize, double maxSpeedRatio) const
{
    // a priming read at the fastest speed - the measured pre-roll (bounded
    // by two windows) plus one block of headroom - or two blocks' worth of
    // input (the FIFO refills one block of headroom on top of the block)
    const auto window = windowFrames > 0 ? windowFrames : 8192;
    const auto prime = 2 * window + static_cast<int> (std::ceil (maxBlockSize * maxSpeedRatio)) + 4096;
    const auto block = static_cast<int> (std::ceil (2.0 * maxBlockSize * maxSpeedRatio)) + 2;
    return juce::jmax (prime, block);
}

int BungeeStretchBackend::primeInputLength (double ratio) const
{
    // enough that, once the pre-roll the raw pipeline emits before input[0]
    // is trimmed, one block of headroom output is left - otherwise the
    // FIFO underruns and the silence it fills in re-inserts the very delay
    // the trim removed
    const auto offset = const_cast<BungeeStretchBackend*> (this)->measureStartOffset (ratio);
    const auto preRoll = juce::jmax (0, -offset);
    return juce::jmin (maxInputPerCall,
                       preRoll + static_cast<int> (std::ceil (headroom * ratio)) + 64);
}

void BungeeStretchBackend::pushRendered (int rendered)
{
    const auto trim = juce::jmin (pendingDiscard, rendered);
    pendingDiscard -= trim;

    if (rendered > trim)
    {
        const float* pointers[64];
        jassert (numChannels <= 64);
        for (int channel = 0; channel < numChannels; ++channel)
            pointers[channel] = renderScratch.getReadPointer (channel) + trim;
        fifo.push (pointers, rendered - trim);
    }
}

void BungeeStretchBackend::primeRaw (const float* const* input, int numInput, double ratio)
{
    impl->stream->reset();
    fifo.clear();
    pendingDiscard = 0;

    const auto rendered = impl->stream->process (input, renderScratch.getArrayOfWritePointers(),
                                                 numInput, static_cast<double> (numInput) / ratio);
    pushRendered (rendered);
}

int BungeeStretchBackend::measureStartOffset (double ratio)
{
    if (calibrator == nullptr)
        return 0;

    if (ratio == calibratedRatio)
        return calibratedOffset;

    // A tone starting right after the priming input (a DC step would be
    // swallowed by the phase vocoder): the raw pipeline's first output
    // frame k0 above half amplitude maps to input stepAt, so its output
    // frame 0 maps to stepAt - k0 * ratio.
    const auto stepAt = windowFrames + 4096;
    const auto total = stepScratch.getNumSamples();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* step = stepScratch.getWritePointer (channel);
        for (int i = 0; i < total; ++i)
            step[i] = i >= stepAt ? std::sin (static_cast<float> (i - stepAt) * 0.2f) : 0.0f;
    }

    const auto* const* stepPointers = stepScratch.getArrayOfReadPointers();
    calibrator->primeRaw (stepPointers, stepAt, ratio);

    auto consumed = stepAt;
    auto firstNonZero = -1;
    const auto maxOutput = static_cast<int> (total / ratio) + preparedBlockSize;

    for (int base = 0; base < maxOutput && firstNonZero < 0; base += preparedBlockSize)
    {
        const auto wanted = juce::jmin (calibrator->inputForOutput (preparedBlockSize, ratio), total - consumed);
        const float* pointers[64];
        for (int channel = 0; channel < numChannels; ++channel)
            pointers[channel] = stepPointers[channel] + consumed;

        calibrator->process (pointers, juce::jmax (0, wanted), calibrationOut.getArrayOfWritePointers(),
                             preparedBlockSize, ratio);
        consumed += juce::jmax (0, wanted);

        const auto* out = calibrationOut.getReadPointer (0);
        for (int i = 0; i < preparedBlockSize; ++i)
            if (std::abs (out[i]) > 0.5f)
            {
                firstNonZero = base + i;
                break;
            }
    }

    calibratedRatio = ratio;
    calibratedOffset = firstNonZero >= 0
                           ? static_cast<int> (std::lround (stepAt - firstNonZero * ratio))
                           : 0;
    return calibratedOffset;
}

void BungeeStretchBackend::prime (const float* const* input, int numInput, double ratio)
{
    const auto offset = measureStartOffset (ratio);

    impl->stream->reset();
    fifo.clear();

    // line output frame 0 up with input[0]: pad the input the pipeline
    // skips, or trim the output it emits ahead of the start
    pendingDiscard = offset < 0 ? static_cast<int> (std::lround (-offset / ratio)) : 0;

    if (offset > 0)
    {
        const auto pad = juce::jmin (offset, zeros.getNumSamples());
        const auto rendered = impl->stream->process (zeros.getArrayOfReadPointers(),
                                                     renderScratch.getArrayOfWritePointers(),
                                                     pad, static_cast<double> (pad) / ratio);
        pushRendered (rendered);
    }

    const auto rendered = impl->stream->process (input, renderScratch.getArrayOfWritePointers(),
                                                 numInput, static_cast<double> (numInput) / ratio);
    pushRendered (rendered);
}

int BungeeStretchBackend::inputForOutput (int numOutput, double ratio)
{
    const auto needed = numOutput + headroom - fifo.getNumStored();
    const auto wanted = needed > 0 ? static_cast<int> (std::ceil (needed * ratio)) : 0;
    return juce::jmin (wanted, maxInputPerCall);
}

void BungeeStretchBackend::process (const float* const* input, int numInput,
                                    float* const* output, int numOutput, double ratio)
{
    if (numInput > 0)
    {
        const auto wantedOutput = static_cast<double> (numInput) / ratio;
        const auto rendered = impl->stream->process (input, renderScratch.getArrayOfWritePointers(),
                                                     numInput, wantedOutput);
        pushRendered (rendered);
    }

    fifo.pop (output, numOutput);
}

} // namespace audium

#endif // STRETCH_BUNGEE_ENABLED
