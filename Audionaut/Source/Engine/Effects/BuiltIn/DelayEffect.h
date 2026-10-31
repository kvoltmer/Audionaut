//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <vector>

#include "Engine/Effects/Effect.h"

namespace audium
{

/**
 * Delay: one feedback delay line per channel (time, feedback, wet mix), the
 * kind that goes on an aux bus. The ring buffer is a power of two read with
 * a mask and 4-point cubic (Hermite) interpolation, after the delays of the
 * Cascade engine; the delay time glides over 50 ms when it changes (the
 * tape-style pitch slide instead of a click), feedback and mix ramp over
 * the same time. The buffer holds the longest time at the prepared sample
 * rate, so a time change never reallocates.
 */
class DelayEffect final : public Effect
{
public:
    enum Parameter { timeMs = 0, feedback, mix };

    static constexpr std::array<EffectParameterInfo, 3> parameterInfos {{
        { "time_ms",  "Time",     "ms", 1.0f, 2000.0f, 350.0f },
        { "feedback", "Feedback", "",   0.0f,    0.95f,  0.4f },
        { "mix",      "Mix",      "",   0.0f,    1.0f,   0.5f },
    }};

    static EffectTypeInfo typeInfo() { return { "builtin:delay", "Delay" }; }

    DelayEffect() : Effect (typeInfo(), parameterInfos) {}

    void prepare (double sampleRate_, int maxBlockSize, int numChannels) override
    {
        sampleRate = sampleRate_;
        maxDelaySamples = static_cast<float> (parameterInfos[timeMs].max * 0.001 * sampleRate);

        // room for the longest delay plus a block, so a read never overtakes the write
        const auto needed = static_cast<int> (std::ceil (maxDelaySamples)) + maxBlockSize + 4;
        auto size = 1;
        while (size < needed)
            size <<= 1;
        mask = size - 1;
        lines.assign (static_cast<size_t> (numChannels), std::vector<float> (static_cast<size_t> (size), 0.0f));

        const auto rampSeconds = 0.05;
        delaySamples.reset (sampleRate, rampSeconds);
        feedbackGain.reset (sampleRate, rampSeconds);
        wetMix.reset (sampleRate, rampSeconds);
        applyParameters();
        reset();
    }

    void release() override
    {
        lines.clear();
        mask = 0;
    }

    void reset() noexcept override
    {
        for (auto& line : lines)
            std::fill (line.begin(), line.end(), 0.0f);
        writeIndex = 0;
        primed = false;
    }

    void process (juce::dsp::AudioBlock<float>& block) noexcept override
    {
        if (lines.empty())
            return;

        if (takeParameterChange())
            applyParameters();

        if (! primed)
        {
            // the first block after prepare() or reset() starts at the set
            // values instead of gliding there from the previous ones
            delaySamples.setCurrentAndTargetValue (delaySamples.getTargetValue());
            feedbackGain.setCurrentAndTargetValue (feedbackGain.getTargetValue());
            wetMix.setCurrentAndTargetValue (wetMix.getTargetValue());
            primed = true;
        }

        const auto numChannels = std::min (block.getNumChannels(), lines.size());
        const auto numSamples = block.getNumSamples();
        auto write = writeIndex;

        for (size_t i = 0; i < numSamples; ++i)
        {
            const auto delay = delaySamples.getNextValue();
            const auto fb = feedbackGain.getNextValue();
            const auto wet = wetMix.getNextValue();
            const auto dry = 1.0f - wet;

            // the read position, `delay` samples behind the write, split
            // into the sample before it and the fraction towards the next;
            // the interpolation takes the neighbour on each side as well
            const auto readPosition = static_cast<float> (write) - delay;
            const auto whole = static_cast<int> (std::floor (readPosition));
            const auto fraction = readPosition - static_cast<float> (whole);
            const auto indexM1 = (whole - 1) & mask;
            const auto index0 = whole & mask;
            const auto index1 = (whole + 1) & mask;
            const auto index2 = (whole + 2) & mask;

            for (size_t c = 0; c < numChannels; ++c)
            {
                auto* line = lines[c].data();
                auto* samples = block.getChannelPointer (c);
                const auto input = samples[i];
                const auto delayed = cubicInterpolate (fraction, line[indexM1], line[index0], line[index1], line[index2]);
                line[write] = input + fb * delayed;
                samples[i] = dry * input + wet * delayed;
            }

            write = (write + 1) & mask;
        }

        writeIndex = write;
    }

    /// Roughly how long the echoes take to fall 60 dB: the time, times the repeats the feedback allows.
    double getTailSeconds() const noexcept override
    {
        const auto seconds = getParameter (timeMs) * 0.001;
        const auto fb = getParameter (feedback);
        if (fb <= 0.0f)
            return seconds;
        return seconds * std::ceil (std::log (0.001) / std::log (static_cast<double> (fb)));
    }

private:
    /// 4-point, 3rd-order Hermite between x0 and x1 at fraction f (the Cascade engine's cubicInterp); f = 0 gives x0 exactly.
    static float cubicInterpolate (float f, float xm1, float x0, float x1, float x2) noexcept
    {
        const auto c = (x1 - xm1) * 0.5f;
        const auto v = x0 - x1;
        const auto w = c + v;
        const auto a = w + v + (x2 - x0) * 0.5f;
        const auto b = w + a;
        return (((a * f) - b) * f + c) * f + x0;
    }

    void applyParameters() noexcept
    {
        // at least two samples, so the point past the read never is the one being written
        const auto samples = static_cast<float> (getParameter (timeMs) * 0.001 * sampleRate);
        delaySamples.setTargetValue (juce::jlimit (2.0f, maxDelaySamples, samples));
        feedbackGain.setTargetValue (getParameter (feedback));
        wetMix.setTargetValue (getParameter (mix));
    }

    double sampleRate = 44100.0;
    float maxDelaySamples = 1.0f;
    std::vector<std::vector<float>> lines;   ///< one ring buffer per channel, a power of two long
    int mask = 0;
    int writeIndex = 0;
    bool primed = false;

    juce::SmoothedValue<float> delaySamples;
    juce::SmoothedValue<float> feedbackGain;
    juce::SmoothedValue<float> wetMix;
};

} // namespace audium
