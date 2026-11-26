//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

// before JUCE, whose names (Point, AudioBuffer) the system's headers would stumble over
#if defined (__APPLE__)
 #include <TargetConditionals.h>
 #if TARGET_OS_OSX
  #include <AudioToolbox/AudioToolbox.h>
 #endif
#endif

#include "HostedEffect.h"

#include <cmath>

#include "ForeignPluginState.h"
#include "PluginTypeId.h"
#include "Engine/Core/MessageThreadAssert.h"

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
    if (instance != nullptr)
        takeInstance (std::move (instance));
}

HostedEffect::HostedEffect (EffectTypeInfo type, juce::PluginDescription description_, Loading) :
    Effect (std::move (type), noParameters),
    description (std::move (description_)),
    status (Status::loading)
{
}

void HostedEffect::takeInstance (std::unique_ptr<juce::AudioPluginInstance> created)
{
    instance = std::move (created);
    buildParameterTable();
    instance->addListener (this);
    instance->setPlayHead (playHead.transport != nullptr ? &playHead : nullptr);
    if (offlineRendering)
        tellPluginRenderMode();   // a plugin that arrives during a bounce
}

void HostedEffect::setOfflineRendering (bool offline)
{
    if (offlineRendering == offline)
        return;

    offlineRendering = offline;
    if (instance == nullptr)
        return;   // a placeholder: the plugin is told when it comes

    tellPluginRenderMode();

    // a VST3 takes the mode when it is set up: the slot prepares it again. The other formats have it
    // by now, and a prepare would only cost them time, twice per bounce
    if (preparedChannels > 0 && description.pluginFormatName == "VST3")
        prepareNeeded = true;
}

void HostedEffect::tellPluginRenderMode()
{
    instance->setNonRealtime (offlineRendering);

   #if JUCE_MAC && JUCE_PLUGINHOST_AU
    // the flag stays in the wrapper for an Audio Unit: the unit itself reads this property
    if (const auto* client = instance->getAudioUnitClient())
        if (auto unit = client->getAudioUnitHandle())
        {
            UInt32 offline = offlineRendering ? 1 : 0;
            AudioUnitSetProperty (unit, kAudioUnitProperty_OfflineRender, kAudioUnitScope_Global, 0, &offline, sizeof (offline));   // a unit without the property says so, and renders as ever
        }
   #endif
}

void HostedEffect::setRenderTransport (std::shared_ptr<const RenderTransport> transport)
{
    playHead.transport = std::move (transport);
    if (instance != nullptr)
        instance->setPlayHead (playHead.transport != nullptr ? &playHead : nullptr);
}

juce::Optional<juce::AudioPlayHead::PositionInfo> HostedEffect::PlayHead::getPosition() const
{
    if (transport == nullptr)
        return {};

    const auto block = transport->read();

    PositionInfo info;
    info.setIsPlaying (block.playing);
    info.setTimeInSamples (static_cast<juce::int64> (block.positionSamples));
    info.setTimeInSeconds (block.positionSeconds);
    info.setPpqPosition (block.positionBeats);
    info.setBpm (block.tempo);
    info.setIsLooping (block.looping);
    if (block.looping)
        info.setLoopPoints (juce::AudioPlayHead::LoopPoints { block.loopStartBeats, block.loopEndBeats });
    return info;
}

void HostedEffect::adopt (std::unique_ptr<juce::AudioPluginInstance> created, juce::String error_)
{
    AUDIUM_ASSERT_MESSAGE_THREAD   // a format may answer a creation on any thread: PluginFormats::createOnMessageThread brings it here
    jassert (getStatus() == Status::loading && instance == nullptr);

    if (created == nullptr)
    {
        error = error_.isNotEmpty() ? std::move (error_) : juce::String ("the plugin could not be loaded");
        status.store (Status::notLoaded, std::memory_order_release);
        return;
    }

    takeInstance (std::move (created));
    error.clear();
    stateDirty.store (true, std::memory_order_relaxed);   // nothing read from this plugin yet
    translatedFrom.clear();
    if (! keptBlob.empty())
        setState (json { { "blob", std::exchange (keptBlob, {}) } });   // what the project saved, into the plugin

    // the slot that holds this is prepared without a plugin: it prepares again before the next plan;
    // a render thread that sees the new status meanwhile finds nothing prepared and passes through
    prepareNeeded = true;
    status.store (Status::loaded, std::memory_order_release);
    if (onLatencyChanged)
        onLatencyChanged();
}

void HostedEffect::expectPlugin()
{
    jassert (instance == nullptr);
    error.clear();
    status.store (Status::loading, std::memory_order_release);
}

HostedEffect::~HostedEffect()
{
    aliveToken.reset();   // a creation callback still in the queue finds nobody
    cancelPendingUpdate();   // nor does a latency report
    if (instance != nullptr)
    {
        instance->removeListener (this);
        instance->setPlayHead (nullptr);
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
    noteStateChanged();
    ++hostEdits;
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
    pluginChannels = numChannels;
    if (description.isInstrument || instance->getBusCount (true) == 0 || instance->getBusCount (false) == 0)
        return false;

    instance->disableNonMainBuses();   // sidechains and extra outputs off: the chain has one signal

    // a bus the plugin would not switch off (an Audio Unit's sidechain input, as a rule) stays in the
    // layout, and its channels are expected in the block after the main bus's: process() gives it
    // silence there, a sidechain nothing is connected to
    auto tryLayout = [this] (const juce::AudioChannelSet& set)
    {
        auto layout = instance->getBusesLayout();
        layout.inputBuses.set (0, set);
        layout.outputBuses.set (0, set);
        if (! instance->setBusesLayout (layout)
            || instance->getChannelCountOfBus (true, 0) != set.size()
            || instance->getChannelCountOfBus (false, 0) != set.size())
            return false;
        pluginChannels = juce::jmax (set.size(), instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels());
        return true;
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

    const auto negotiated = negotiateLayout (numChannels);
    if (! negotiated)
        error = juce::String (numChannels) + (numChannels == 1 ? " channel" : " channels") + " not supported";
    else
        error.clear();
    status.store (negotiated ? Status::loaded : Status::incompatibleLayout, std::memory_order_release);
    prepareNeeded = false;

    if (! negotiated)
        pluginChannels = numChannels;   // never run: the sizes below only have to be sane
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
    // a render thread may be here while the plugin of a loading placeholder arrives on the message
    // thread: the status is published after the instance, so it is read first, as in process()
    if (status.load (std::memory_order_acquire) != Status::loaded || preparedChannels == 0 || instance == nullptr)
        return;
    instance->reset();
}

void HostedEffect::process (juce::dsp::AudioBlock<float>& block) noexcept
{
    if (status.load (std::memory_order_acquire) != Status::loaded || preparedChannels == 0 || instance == nullptr)
        return;   // passes through: a placeholder (loading or not), or a layout the plugin refused

    const auto numChannels = static_cast<int> (block.getNumChannels());
    const auto numSamples = static_cast<int> (block.getNumSamples());
    if (numChannels != preparedChannels || numSamples > preparedBlockSize)
        return;

    midi.clear();

    // the channels of the buses the plugin kept beside the main one: silence in, whatever comes out dropped
    const auto mainChannels = foldMono ? 2 : numChannels;
    for (auto channel = mainChannels; channel < pluginChannels; ++channel)
        scratch.clear (channel, 0, numSamples);

    if (foldMono)
    {
        auto* mono = block.getChannelPointer (0);
        scratch.copyFrom (0, 0, mono, numSamples);
        scratch.copyFrom (1, 0, mono, numSamples);
        wrapper.setDataToReferTo (scratch.getArrayOfWritePointers(), pluginChannels, numSamples);
        instance->processBlock (wrapper, midi);
        const auto* left = scratch.getReadPointer (0);
        const auto* right = scratch.getReadPointer (1);
        for (auto i = 0; i < numSamples; ++i)
            mono[i] = 0.5f * (left[i] + right[i]);
        return;
    }

    for (auto channel = 0; channel < numChannels; ++channel)
        pointers[static_cast<size_t> (channel)] = block.getChannelPointer (static_cast<size_t> (channel));
    for (auto channel = numChannels; channel < pluginChannels; ++channel)
        pointers[static_cast<size_t> (channel)] = scratch.getWritePointer (channel);
    wrapper.setDataToReferTo (pointers.data(), pluginChannels, numSamples);
    instance->processBlock (wrapper, midi);
}

int HostedEffect::getLatencySamples() const noexcept
{
    return instance != nullptr && getStatus() == Status::loaded ? instance->getLatencySamples() : 0;
}

double HostedEffect::getTailSeconds() const noexcept
{
    if (instance == nullptr || getStatus() != Status::loaded)
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

    if (blob.empty())
        return;   // the state of a placeholder that never had the plugin: nothing to give this one

    if (! stateDirty.load (std::memory_order_acquire) && blob == cachedBlob)
        return;   // what the plugin already has: an undo replay or a no-op read
    if (blob == translatedFrom && ! changedSinceTranslation.load (std::memory_order_acquire))
        return;   // the same for another format's state: the plugin was given it, translated, and has not moved since

    juce::MemoryBlock chunk;
    if (! chunk.fromBase64Encoding (blob))
        return;

    // a substitute for the plugin of another format (PluginHost::findSubstitute) is handed that format's
    // state: a VST3 ignores an Audio Unit's property list and a unit a VST3's xml, so each gets the
    // plugin's chunk out of the other's state, in the wrapping it reads
    using Kind = ForeignPluginState::Translation::Kind;
    const auto translation = ForeignPluginState::forPlugin (chunk, description.pluginFormatName, description.fileOrIdentifier);
    if (translation.kind == Kind::translated)
    {
        instance->setStateInformation (translation.state.getData(), static_cast<int> (translation.state.getSize()));
        cachedBlob.clear();
        stateDirty.store (true, std::memory_order_release);   // the state to save is the plugin's own from here on
        translatedFrom = blob;
        changedSinceTranslation.store (false, std::memory_order_release);
        ++hostEdits;
        return;
    }

    translatedFrom.clear();
    if (translation.kind == Kind::unreadable)
    {
        // nothing in it this plugin can be given, so the plugin stays as it is. The blob is kept as the
        // state to save until the plugin is changed: saved untouched, the project still opens with these
        // settings where the plugin is in the format that wrote them
        cachedBlob = blob;
        stateDirty.store (false, std::memory_order_release);
        return;
    }

    instance->setStateInformation (chunk.getData(), static_cast<int> (chunk.getSize()));
    cachedBlob = blob;
    stateDirty.store (false, std::memory_order_release);
    ++hostEdits;
}

json HostedEffect::getPluginInfo() const
{
    return PluginTypeId::pluginInfo (description);
}

//==============================================================================
void HostedEffect::audioProcessorParameterChanged (juce::AudioProcessor*, int, float)
{
    noteStateChanged();
    pluginChanges.fetch_add (1, std::memory_order_relaxed);
}

void HostedEffect::audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int)
{
    openGestures.fetch_add (1, std::memory_order_relaxed);
}

void HostedEffect::audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int)
{
    openGestures.fetch_sub (1, std::memory_order_relaxed);
    pluginChanges.fetch_add (1, std::memory_order_relaxed);   // the gesture's end is news too: what waited for it goes on
}

void HostedEffect::audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details)
{
    noteStateChanged();
    if (details.parameterInfoChanged || details.programChanged || details.nonParameterStateChanged)
        pluginChanges.fetch_add (1, std::memory_order_relaxed);

    // any thread, a render thread among them (a format that reads the latency while it processes):
    // no allocation here, the chain hears of it on the message thread
    if (details.latencyChanged)
        triggerAsyncUpdate();
}

void HostedEffect::handleAsyncUpdate()
{
    if (onLatencyChanged)
        onLatencyChanged();
}

} // namespace audium
