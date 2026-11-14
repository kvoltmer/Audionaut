//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <cmath>
#include <functional>
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
    const char* group = nullptr;   ///< the section an editor shows it in ("Mid"); consecutive parameters share one
    const char* label = nullptr;   ///< its name within that section ("Freq"); the name when unset
    bool movesLatency = false;     ///< a change moves getLatencySamples(), which rebuilds the plan: an editor writes it at a drag's end only
};

/// What an effect type is called: the id is what projects store ("builtin:eq3"), the name what menus show,
/// the category the submenu it is listed in ("Reverb"), the description a sentence on what it does.
struct EffectTypeInfo
{
    std::string id;
    std::string name;
    std::string category = {};
    std::string description = {};
};

/**
 * One effect instance: the contract the insert chains, the renderer and the
 * surfaces (CLI, editors) program against, for built-ins and hosted plugins
 * alike. The parameter table below is the built-ins': a fixed span of infos
 * and an atomic per value. A hosted plugin has its parameters only once it
 * is loaded, and more of them, so the parameter methods are virtual and a
 * hosted effect answers them from its own table.
 *
 * Threads: prepare(), release() and the state round trip run on the message
 * thread while nothing renders the effect (the planner sees to that).
 * prepare() can come again on the same instance - another sample rate,
 * another block size, another channel count - and the effect sizes itself
 * afresh; process() takes any block up to the prepared size. reset() and
 * process() run on whichever render thread takes the job, one at a time.
 * Parameters are atomics inside the effect: setParameter() is a
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

    /// Message thread: the effect changed in a way prepare() has to see again (a plugin that arrived after its
    /// placeholder stood in a prepared slot): the slot prepares it anew before the next plan runs it.
    virtual bool needsPrepare() const noexcept { return false; }

    /// The effect turns the level down and can say by how much: an editor shows a gain reduction meter beside the dials.
    virtual bool hasGainReductionMeter() const noexcept { return false; }

    /// Message thread: the most gain reduction, in dB as a positive figure, any sample saw since the last call. The
    /// call clears it, so a meter polling this shows the peak of each interval and falls to zero while nothing renders
    /// the effect.
    virtual float takeGainReductionDb() noexcept { return 0.0f; }

    virtual std::span<const EffectParameterInfo> getParameters() const noexcept { return parameters; }
    virtual int getNumParameters() const noexcept { return static_cast<int> (parameters.size()); }

    /// The index of the parameter with this id, -1 if there is none.
    virtual int getParameterIndex (std::string_view id) const noexcept
    {
        for (size_t i = 0; i < parameters.size(); ++i)
            if (id == parameters[i].id)
                return static_cast<int> (i);
        return -1;
    }

    /// Message thread: the value as the effect itself reads it ("-6.00 dB", "Nuke"), for editors and agents;
    /// empty when the plain number with the parameter's unit says it.
    virtual juce::String getParameterText (int index, float value) const
    {
        juce::ignoreUnused (index, value);
        return {};
    }

    /// Message thread: typed text back to a value, the inverse of getParameterText(); false when the effect
    /// has no reading of its own (the text is then taken as the plain number).
    virtual bool parseParameterText (int index, const juce::String& text, float& value) const
    {
        juce::ignoreUnused (index, text, value);
        return false;
    }

    virtual float getParameter (int index) const noexcept
    {
        return isValidIndex (index) ? values[static_cast<size_t> (index)].load (std::memory_order_relaxed) : 0.0f;
    }

    /// The value as setParameter() would store it: within the parameter's range, a boolean at 0 or 1.
    virtual float clampParameter (int index, float value) const noexcept
    {
        if (! isValidIndex (index))
            return 0.0f;

        const auto& info = parameters[static_cast<size_t> (index)];
        return info.boolean ? (value >= 0.5f ? 1.0f : 0.0f)
                            : juce::jlimit (info.min, info.max, value);
    }

    /// Clamps to the parameter's range (a boolean to 0 or 1) and stores; the next block picks it up.
    virtual void setParameter (int index, float value) noexcept
    {
        if (! isValidIndex (index))
            return;

        values[static_cast<size_t> (index)].store (clampParameter (index, value), std::memory_order_relaxed);
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

    /// A hosted plugin: what the project writes beside the type so another machine can find the plugin
    /// ({"name", "manufacturer", "format", "file", "version"}); null for a built-in.
    virtual json getPluginInfo() const { return nullptr; }

    /// Message thread: the effect's latency moved of its own accord (a plugin's report), not through a
    /// parameter this engine set - or it needs preparing again (needsPrepare). The chain holding the slot
    /// sets it to ask for a plan; a bare effect has none.
    std::function<void()> onLatencyChanged;

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
