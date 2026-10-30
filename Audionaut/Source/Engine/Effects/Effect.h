//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <cmath>
#include <span>
#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace audium
{

/// One parameter of an effect: a stable id for persistence and the CLI, a name and unit for displays, and its range.
struct EffectParameterInfo
{
    const char* id;
    const char* name;
    const char* unit;
    float min;
    float max;
    float def;
    bool boolean = false;   ///< 0 or 1, shown as a toggle
};

/// What an effect type is called: the id is what projects store ("builtin:eq3"), the name what menus show.
struct EffectTypeInfo
{
    std::string id;
    std::string name;
};

/**
 * One effect instance: the contract the insert chains, the renderer and the
 * surfaces (CLI, editors) program against, for built-ins and, later, hosted
 * plugins alike.
 *
 * Threads: prepare(), release() and the state round trip run on the message
 * thread while no live plan uses the effect (the planner sees to that);
 * reset() and process() run on whichever render thread takes the job, one at
 * a time. Parameters are atomics inside the effect: setParameter() is a
 * store from the message thread or the CLI, process() reads them at the
 * start of a block. Smoothing is the effect's own business.
 *
 * getState() is deterministic for a given parameter set (fixed key order,
 * values rounded), because the model compares its JSON with what it last
 * wrote to skip a read that changes nothing.
 */
class Effect
{
public:
    static constexpr int maxParameters = 16;

    Effect (EffectTypeInfo type_, std::span<const EffectParameterInfo> parameters_) :
        type (std::move (type_)),
        parameters (parameters_)
    {
        jassert (parameters.size() <= static_cast<size_t> (maxParameters));
        for (size_t i = 0; i < parameters.size(); ++i)
            values[i].store (parameters[i].def, std::memory_order_relaxed);
    }

    virtual ~Effect() = default;

    const EffectTypeInfo& getType() const noexcept { return type; }

    /// Message thread: sizes the effect for the format it will process. numChannels is fixed until the next prepare().
    virtual void prepare (double sampleRate, int maxBlockSize, int numChannels) = 0;

    /// Message thread: frees what prepare() allocated; the effect processes nothing until prepared again.
    virtual void release() {}

    /// Render thread: clears delay lines and envelopes, keeps the parameters.
    virtual void reset() noexcept = 0;

    /// Render thread: in place, as many channels as prepared, at most maxBlockSize samples.
    virtual void process (juce::dsp::AudioBlock<float>& block) noexcept = 0;

    virtual int getLatencySamples() const noexcept { return 0; }
    virtual double getTailSeconds() const noexcept { return 0.0; }

    std::span<const EffectParameterInfo> getParameters() const noexcept { return parameters; }
    int getNumParameters() const noexcept { return static_cast<int> (parameters.size()); }

    /// The index of the parameter with this id, -1 if there is none.
    int getParameterIndex (std::string_view id) const noexcept
    {
        for (size_t i = 0; i < parameters.size(); ++i)
            if (id == parameters[i].id)
                return static_cast<int> (i);
        return -1;
    }

    float getParameter (int index) const noexcept
    {
        return isValidIndex (index) ? values[static_cast<size_t> (index)].load (std::memory_order_relaxed) : 0.0f;
    }

    /// Clamps to the parameter's range (a boolean to 0 or 1) and stores; the next block picks it up.
    void setParameter (int index, float value) noexcept
    {
        if (! isValidIndex (index))
            return;

        const auto& info = parameters[static_cast<size_t> (index)];
        const auto clamped = info.boolean ? (value >= 0.5f ? 1.0f : 0.0f)
                                          : juce::jlimit (info.min, info.max, value);
        values[static_cast<size_t> (index)].store (clamped, std::memory_order_relaxed);
        parameterVersion.fetch_add (1, std::memory_order_release);
    }

    /// {id: value} for every parameter, in id order, values rounded to a millionth: the same set gives the same JSON.
    virtual json getState() const
    {
        auto state = json::object();
        for (size_t i = 0; i < parameters.size(); ++i)
            state[parameters[i].id] = roundForState (values[i].load (std::memory_order_relaxed));
        return state;
    }

    /// The inverse of getState(); unknown keys are ignored, missing ones keep their value.
    virtual void setState (const json& state)
    {
        if (! state.is_object())
            return;

        for (size_t i = 0; i < parameters.size(); ++i)
            if (auto it = state.find (parameters[i].id); it != state.end() && it->is_number())
                setParameter (static_cast<int> (i), it->get<float>());
    }

protected:
    /// Render thread: the version the last block saw against the current one, so an effect recomputes only on a change.
    bool takeParameterChange() noexcept
    {
        const auto version = parameterVersion.load (std::memory_order_acquire);
        if (version == seenVersion)
            return false;
        seenVersion = version;
        return true;
    }

    static float roundForState (float value) noexcept
    {
        return static_cast<float> (std::round (static_cast<double> (value) * 1.0e6) / 1.0e6);
    }

private:
    bool isValidIndex (int index) const noexcept
    {
        return index >= 0 && index < static_cast<int> (parameters.size());
    }

    EffectTypeInfo type;
    std::span<const EffectParameterInfo> parameters;
    std::array<std::atomic<float>, maxParameters> values = {};
    std::atomic<std::uint32_t> parameterVersion { 1 };
    std::uint32_t seenVersion = 0;   // render thread only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Effect)
};

} // namespace audium
