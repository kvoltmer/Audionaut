//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "HostedEffect.h"

#include <cmath>

#include "PluginTypeId.h"

namespace audium
{

namespace
{
    constexpr std::array<EffectParameterInfo, 0> noParameters {};
}

HostedEffect::HostedEffect (EffectTypeInfo type, juce::PluginDescription description_,
                            std::unique_ptr<juce::AudioPluginInstance> instance_, juce::String error_) :
    Effect (std::move (type), noParameters),
    description (std::move (description_)),
    instance (std::move (instance_)),
    status (instance != nullptr ? Status::loaded : Status::notLoaded),
    error (std::move (error_))
{
    if (instance == nullptr)
        return;

    buildParameterTable();
    instance->addListener (this);
}

HostedEffect::~HostedEffect()
{
    aliveToken.reset();   // a latency callback still in the queue finds nobody
    if (instance != nullptr)
    {
        instance->removeListener (this);
        if (preparedChannels > 0)
            instance->releaseResources();
        instance.reset();
    }
}

//==============================================================================
void HostedEffect::buildParameterTable()
{
    const auto& all = instance->getParameters();
    parameters.assign (all.begin(), all.end());

    // the strings first, all of them, so the pointers the infos take stay put
    ids.reserve (parameters.size());
    names.reserve (parameters.size());
    units.reserve (parameters.size());
    for (auto* parameter : parameters)
    {
        std::string id;
        if (auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*> (parameter))
            id = hosted->getParameterID().toStdString();
        if (id.empty())
            id = "p" + std::to_string (parameter->getParameterIndex());
        ids.push_back (std::move (id));
        names.push_back (parameter->getName (64).toStdString());
        units.push_back (parameter->getLabel().toStdString());
    }

    infos.reserve (parameters.size());
    for (size_t i = 0; i < parameters.size(); ++i)
    {
        infos.push_back ({ ids[i].c_str(), names[i].c_str(), units[i].c_str(),
                           0.0f, 1.0f, juce::jlimit (0.0f, 1.0f, parameters[i]->getDefaultValue()),
                           parameters[i]->isBoolean() });
    }
}

juce::AudioProcessorParameter* HostedEffect::parameterAt (int index) const noexcept
{
    return index >= 0 && index < static_cast<int> (parameters.size()) ? parameters[static_cast<size_t> (index)] : nullptr;
}

int HostedEffect::getParameterIndex (std::string_view id) const noexcept
{
    for (size_t i = 0; i < ids.size(); ++i)
        if (id == ids[i])
            return static_cast<int> (i);
    return -1;
}

float HostedEffect::getParameter (int index) const noexcept
{
    auto* parameter = parameterAt (index);
    return parameter != nullptr ? parameter->getValue() : 0.0f;
}

float HostedEffect::clampParameter (int index, float value) const noexcept
{
    auto* parameter = parameterAt (index);
    if (parameter == nullptr)
        return 0.0f;
    return parameter->isBoolean() ? (value >= 0.5f ? 1.0f : 0.0f) : juce::jlimit (0.0f, 1.0f, value);
}

void HostedEffect::setParameter (int index, float value) noexcept
{
    auto* parameter = parameterAt (index);
    if (parameter == nullptr)
        return;

    // the host's call: the plugin's wrapper hands the value to the processor
    // and its own window; the state changed with it
    parameter->setValue (clampParameter (index, value));
    stateDirty.store (true, std::memory_order_relaxed);
}

juce::String HostedEffect::getParameterText (int index, float value) const
{
    auto* parameter = parameterAt (index);
    return parameter != nullptr ? parameter->getText (clampParameter (index, value), 32) : juce::String();
}

bool HostedEffect::parseParameterText (int index, const juce::String& text, float& value) const
{
    auto* parameter = parameterAt (index);
    if (parameter == nullptr)
        return false;
    value = parameter->getValueForText (text);
    return true;
}

//==============================================================================
bool HostedEffect::negotiateLayout (int numChannels)
{
    foldMono = false;
    if (description.isInstrument || instance->getBusCount (true) == 0 || instance->getBusCount (false) == 0)
        return false;

    instance->disableNonMainBuses();   // sidechains and extra outputs off: the chain has one signal

    auto tryLayout = [this] (const juce::AudioChannelSet& set)
    {
        auto layout = instance->getBusesLayout();
        layout.inputBuses.set (0, set);
        layout.outputBuses.set (0, set);
        return instance->setBusesLayout (layout);
    };

    if (tryLayout (juce::AudioChannelSet::canonicalChannelSet (numChannels))
        || tryLayout (juce::AudioChannelSet::discreteChannels (numChannels)))
        return true;

    // a mono chain through a stereo-only plugin: the one channel on both sides, the sides averaged back
    if (numChannels == 1 && tryLayout (juce::AudioChannelSet::stereo()))
    {
        foldMono = true;
        return true;
    }

    return false;
}

void HostedEffect::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    if (instance == nullptr)
        return;

    if (preparedChannels > 0)
        instance->releaseResources();   // a second prepare: the plugin lets go of the old format first

    status = negotiateLayout (numChannels) ? Status::loaded : Status::incompatibleLayout;
    if (status == Status::incompatibleLayout)
        error = juce::String (numChannels) + (numChannels == 1 ? " channel" : " channels") + " not supported";
    else
        error.clear();

    const auto pluginChannels = foldMono ? 2 : numChannels;
    scratch.setSize (pluginChannels, maxBlockSize);
    scratch.clear();
    pointers.assign (static_cast<size_t> (pluginChannels), nullptr);
    wrapper.setDataToReferTo (scratch.getArrayOfWritePointers(), pluginChannels, maxBlockSize);   // sizes the wrapper's channel array once
    midi.ensureSize (64);

    instance->setRateAndBufferSizeDetails (sampleRate, maxBlockSize);
    instance->prepareToPlay (sampleRate, maxBlockSize);
    preparedChannels = numChannels;
    preparedBlockSize = maxBlockSize;
}

void HostedEffect::release()
{
    if (instance != nullptr && preparedChannels > 0)
        instance->releaseResources();
    preparedChannels = 0;
    preparedBlockSize = 0;
}

void HostedEffect::reset() noexcept
{
    if (instance != nullptr && preparedChannels > 0)
        instance->reset();
}

void HostedEffect::process (juce::dsp::AudioBlock<float>& block) noexcept
{
    if (instance == nullptr || status != Status::loaded || preparedChannels == 0)
        return;   // passes through: a placeholder, or a layout the plugin refused

    const auto numChannels = static_cast<int> (block.getNumChannels());
    const auto numSamples = static_cast<int> (block.getNumSamples());
    if (numChannels != preparedChannels || numSamples > preparedBlockSize)
        return;

    midi.clear();

    if (foldMono)
    {
        auto* mono = block.getChannelPointer (0);
        scratch.copyFrom (0, 0, mono, numSamples);
        scratch.copyFrom (1, 0, mono, numSamples);
        wrapper.setDataToReferTo (scratch.getArrayOfWritePointers(), 2, numSamples);
        instance->processBlock (wrapper, midi);
        const auto* left = scratch.getReadPointer (0);
        const auto* right = scratch.getReadPointer (1);
        for (auto i = 0; i < numSamples; ++i)
            mono[i] = 0.5f * (left[i] + right[i]);
        return;
    }

    for (auto channel = 0; channel < numChannels; ++channel)
        pointers[static_cast<size_t> (channel)] = block.getChannelPointer (static_cast<size_t> (channel));
    wrapper.setDataToReferTo (pointers.data(), numChannels, numSamples);
    instance->processBlock (wrapper, midi);
}

int HostedEffect::getLatencySamples() const noexcept
{
    return instance != nullptr && status == Status::loaded ? instance->getLatencySamples() : 0;
}

double HostedEffect::getTailSeconds() const noexcept
{
    if (instance == nullptr || status != Status::loaded)
        return 0.0;
    const auto tail = instance->getTailLengthSeconds();
    return std::isfinite (tail) && tail > 0.0 ? tail : 0.0;   // an endless tail is no figure to extend an export by
}

//==============================================================================
std::string HostedEffect::blobOf (const json& state)
{
    if (state.is_string())
        return state.get<std::string>();
    if (state.is_object())
        if (auto it = state.find ("blob"); it != state.end() && it->is_string())
            return it->get<std::string>();
    return {};
}

json HostedEffect::getState() const
{
    if (instance == nullptr)
        return { { "blob", keptBlob } };

    if (stateDirty.exchange (false, std::memory_order_acq_rel) || cachedBlob.empty())
    {
        juce::MemoryBlock chunk;
        instance->getStateInformation (chunk);
        cachedBlob = chunk.toBase64Encoding().toStdString();
    }
    return { { "blob", cachedBlob } };
}

void HostedEffect::setState (const json& state)
{
    const auto isBlob = state.is_string()
                        || (state.is_object() && state.contains ("blob") && state["blob"].is_string());
    if (! isBlob)
        return;   // not a plugin state (a built-in's {id: value} from a swapped type, or nothing): leave the plugin as it is
    const auto blob = blobOf (state);

    if (instance == nullptr)
    {
        keptBlob = blob;
        return;
    }

    if (! stateDirty.load (std::memory_order_acquire) && blob == cachedBlob)
        return;   // what the plugin already has: an undo replay or a no-op read

    juce::MemoryBlock chunk;
    if (! blob.empty() && ! chunk.fromBase64Encoding (blob))
        return;

    instance->setStateInformation (chunk.getData(), static_cast<int> (chunk.getSize()));
    cachedBlob = blob;
    stateDirty.store (false, std::memory_order_release);
}

json HostedEffect::getPluginInfo() const
{
    return PluginTypeId::pluginInfo (description);
}

//==============================================================================
void HostedEffect::audioProcessorParameterChanged (juce::AudioProcessor*, int, float)
{
    stateDirty.store (true, std::memory_order_relaxed);
}

void HostedEffect::audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details)
{
    stateDirty.store (true, std::memory_order_relaxed);

    if (details.latencyChanged)
    {
        // any thread: on the message thread, if the effect still lives, the chain hears of it
        juce::MessageManager::callAsync ([weak = std::weak_ptr<int> (aliveToken), this]
        {
            if (weak.lock() != nullptr && onLatencyChanged)
                onLatencyChanged();
        });
    }
}

} // namespace audium
