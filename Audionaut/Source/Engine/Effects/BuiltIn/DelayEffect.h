//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <limits>
#include <memory>
#include <vector>

#include "Engine/Effects/Effect.h"
#include "Engine/Playback/RenderTransport.h"

namespace audium
{

/**
 * Delay: one feedback delay line per channel (time, feedback, wet mix), the
 * kind that goes on an aux bus - where it starts fully wet (see InsertChain). The ring buffer is a power of two read with
 * a mask and 4-point cubic (Hermite) interpolation, after the delays of the
 * Cascade engine; the delay time glides over 50 ms when it changes (the
 * tape-style pitch slide instead of a click), feedback and mix ramp over
 * the same time. The buffer holds the longest time at the prepared sample
 * rate, so a time change never reallocates.
 *
 * The time is set in one of four modes. In "ms" it is a time in
 * milliseconds. In Straight, Dotted and Triplets it is a note value, 1/32
 * to 1/1, at the tempo of the transport the effect was made with: the
 * note's length, half as long again, or two thirds of it - and it follows
 * the tempo when that changes. A note value longer than the buffer at a
 * slow tempo is halved until it fits, so the echoes stay in time. An
 * editor shows one Time dial, the reading of the mode in use. A new delay
 * is a dotted eighth.
 *
 * Switching the mode keeps the time as near as the new mode allows: the
 * note value nearest (by ratio) to the time so far, or, into "ms", that
 * time in milliseconds. So the mode moves the time's other reading with
 * it, and an undo step for it is the whole state (setState() stores the
 * values as they are, without that).
 */
class DelayEffect final : public Effect
{
public:
    enum Parameter { timeMs = 0, division, mode, feedback, mix };
    enum Mode { dotted = 0, straight, triplets, milliseconds };

    static constexpr int numDivisions = 6;        ///< 1/32, 1/16, 1/8, 1/4, 1/2, 1/1
    static constexpr double maxSeconds = 4.0;     ///< what the buffer holds

    static constexpr std::array<EffectParameterInfo, 5> parameterInfos {{
        { "time_ms",       "Time",     "ms", 1.0f, 2000.0f, 350.0f },
        { "time_division", "Time",     "",   0.0f, static_cast<float> (numDivisions - 1), 2.0f, false, nullptr, nullptr, false, true },
        { "mode",          "Mode",     "",   0.0f, 3.0f, static_cast<float> (dotted), false, nullptr, nullptr, false, true, true },
        { "feedback",      "Feedback", "",   0.0f,    0.95f,  0.4f },
        { "mix",           "Mix",      "",   0.0f,    1.0f,   0.5f, false, nullptr, nullptr, false, false, false, true },
    }};

    static EffectTypeInfo typeInfo() { return { "builtin:delay", "Delay", "Delay", "A feedback delay with a time in milliseconds or in note values at the project's tempo, feedback and mix." }; }

    /// `transport`: where the tempo is read from (the engine's, see EffectRegistry::getRenderTransport); one
    /// made without it (a test's) runs at a transport of its own, at 120.
    explicit DelayEffect (std::shared_ptr<const RenderTransport> transport_ = std::make_shared<RenderTransport>()) :
        Effect (typeInfo(), parameterInfos),
        transport (std::move (transport_))
    {
        jassert (transport != nullptr);
    }

    /// The delay time as the parameters and the transport's tempo stand.
    double getDelaySeconds() const noexcept
    {
        return secondsOf (currentMode(), getParameter (timeMs), currentDivision(), transport->read().tempo);
    }

    /// The mode takes the time's reading with it (see the class comment); every other parameter is stored as it is.
    void setParameter (int index, float value) noexcept override
    {
        if (index != mode)
        {
            Effect::setParameter (index, value);
            return;
        }

        const auto next = static_cast<int> (clampParameter (mode, value));
        if (next == currentMode())
            return;

        const auto tempo = transport->read().tempo;
        const auto seconds = secondsOf (currentMode(), getParameter (timeMs), currentDivision(), tempo);
        if (next == milliseconds)
            Effect::setParameter (timeMs, static_cast<float> (seconds * 1000.0));
        else
            Effect::setParameter (division, static_cast<float> (nearestDivision (next, seconds, tempo)));
        Effect::setParameter (mode, static_cast<float> (next));
    }

    /// The values as they were saved: a mode read back does not move the time.
    void setState (const json& state) override
    {
        if (! state.is_object())
            return;

        for (size_t i = 0; i < parameterInfos.size(); ++i)
            if (auto it = state.find (parameterInfos[i].id); it != state.end() && it->is_number())
                Effect::setParameter (static_cast<int> (i), it->get<float>());
    }

    bool isParameterShown (int index) const noexcept override
    {
        if (index == timeMs)
            return currentMode() == milliseconds;
        if (index == division)
            return currentMode() != milliseconds;
        return true;
    }

    /// "Dotted" ... "ms" for the mode; "1/4", "1/4." or "1/4T" for a note value, as the mode reads it.
    juce::String getParameterText (int index, float value) const override
    {
        if (index == mode)
            return modeNames[static_cast<size_t> (clampParameter (mode, value))];

        if (index == division)
        {
            const auto text = "1/" + juce::String (32 >> static_cast<int> (clampParameter (division, value)));
            const auto current = currentMode();
            return current == dotted ? text + "." : current == triplets ? text + "T" : text;
        }

        return {};
    }

    bool parseParameterText (int index, const juce::String& text, float& value) const override
    {
        const auto typed = text.trim();

        if (index == mode)
        {
            for (size_t i = 0; i < modeNames.size(); ++i)
            {
                if (typed.isNotEmpty() && juce::String (modeNames[i]).startsWithIgnoreCase (typed))
                {
                    value = static_cast<float> (i);
                    return true;
                }
            }
            return false;
        }

        if (index == division)
        {
            // "1/8", "1/8T", "8": the note, whatever is said about dots and triplets (that is the mode's)
            const auto denominator = typed.fromFirstOccurrenceOf ("/", false, false).isNotEmpty()
                                   ? typed.fromFirstOccurrenceOf ("/", false, false).getIntValue()
                                   : typed.getIntValue();
            for (auto i = 0; i < numDivisions; ++i)
            {
                if (denominator == (32 >> i))
                {
                    value = static_cast<float> (i);
                    return true;
                }
            }
        }

        return false;
    }

    void prepare (double sampleRate_, int maxBlockSize, int numChannels) override
    {
        sampleRate = sampleRate_;
        maxDelaySamples = static_cast<float> (maxSeconds * sampleRate);

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
        applyParameters (transport->read().tempo);
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

        // a note value's time moves with the tempo, which is read once a block
        const auto tempo = transport->read().tempo;
        const auto tempoChanged = ! juce::exactlyEqual (tempo, appliedTempo);
        if (takeParameterChange() || tempoChanged)
            applyParameters (tempo);

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
        if (getParameter (mix) <= 0.0f)
            return 0.0;   // fully dry: the echoes are never heard
        const auto seconds = getDelaySeconds();
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

    static constexpr std::array<const char*, 4> modeNames { "Dotted", "Straight", "Triplets", "ms" };

    int currentMode() const noexcept { return static_cast<int> (getParameter (mode)); }
    int currentDivision() const noexcept { return static_cast<int> (getParameter (division)); }

    /// A note value's length at a tempo, as the mode reads it and before any halving: 1/32 is an eighth of a beat.
    static double noteSeconds (int mode_, int division_, double tempo) noexcept
    {
        const auto beats = 0.125 * static_cast<double> (1 << division_);
        const auto factor = mode_ == dotted ? 1.5 : mode_ == triplets ? 2.0 / 3.0 : 1.0;
        return beats * factor * 60.0 / std::max (1.0, tempo);
    }

    static double secondsOf (int mode_, float milliseconds_, int division_, double tempo) noexcept
    {
        if (mode_ == milliseconds)
            return static_cast<double> (milliseconds_) * 0.001;

        // longer than the buffer: the next shorter time that is still in time
        auto seconds = noteSeconds (mode_, division_, tempo);
        while (seconds > maxSeconds)
            seconds *= 0.5;
        return seconds;
    }

    /// The note value of a mode whose length is nearest to `seconds`, by ratio: an octave off is as far up as down.
    static int nearestDivision (int mode_, double seconds, double tempo) noexcept
    {
        auto nearest = 0;
        auto distance = std::numeric_limits<double>::max();
        for (auto i = 0; i < numDivisions; ++i)
        {
            const auto off = std::abs (std::log (noteSeconds (mode_, i, tempo) / std::max (1.0e-6, seconds)));
            if (off < distance)
            {
                distance = off;
                nearest = i;
            }
        }
        return nearest;
    }

    void applyParameters (double tempo) noexcept
    {
        appliedTempo = tempo;

        // at least two samples, so the point past the read never is the one being written
        const auto samples = static_cast<float> (secondsOf (currentMode(), getParameter (timeMs), currentDivision(), tempo) * sampleRate);
        delaySamples.setTargetValue (juce::jlimit (2.0f, maxDelaySamples, samples));
        feedbackGain.setTargetValue (getParameter (feedback));
        wetMix.setTargetValue (getParameter (mix));
    }

    const std::shared_ptr<const RenderTransport> transport;
    double appliedTempo = 0.0;   // render thread: the tempo the delay time was last set by
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
