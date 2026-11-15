//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include <JuceHeader.h>

#include "Engine/Effects/Effect.h"

namespace audium
{

/**
 * A plugin (Audio Unit, VST3, LV2) behind the Effect contract: the insert
 * chains, the planner and the renderer treat it like a built-in.
 *
 * The plugin's parameters are the effect's, normalised 0..1 as the plugin
 * exposes them, under the ids the plugin gives them ("p<index>" when it
 * gives none); the plugin's own text for a value answers getParameterText.
 * The state is the plugin's state chunk, base64 in {"blob": ..}, read from
 * the plugin only when it reported a change since the last read, so a
 * project save or an undo snapshot that changes nothing gets the same
 * string. Setting a state equal to that string is skipped.
 *
 * prepare() negotiates the channel layout: the chain's channel count on the
 * main input and output, any other bus off. A mono chain with a plugin that
 * only runs in stereo gets the plugin in stereo, fed the one channel on
 * both sides and the two sides averaged back; any other layout the plugin
 * refuses leaves the slot passing the signal through (incompatibleLayout).
 *
 * Without an instance the effect is a placeholder: it keeps the blob it is
 * given, runs nothing and reports why (the plugin failed to load, or this
 * engine lists plugins without loading them, as the scratch engine of a
 * hosted edit does). The project keeps the element either way. A plugin
 * that is created asynchronously (one that runs in a process of its own)
 * starts as a loading placeholder and adopt()s the plugin when it arrives.
 *
 * Threads: as Effect says - prepare, release and the state on the message
 * thread, process on a render thread, one at a time. The plugin's own
 * reports (AudioProcessorListener) can come from any thread: they set
 * flags and counters, and a latency change reaches onLatencyChanged on the
 * message thread without allocating where it was reported.
 *
 * What the user does in the plugin's own window is the plugin's to report:
 * getPluginChangeCount() moves with every such report, isGestureOpen() says
 * a control is held, and getHostEditCount() moves when the host itself put
 * a value or a state into the plugin - whose echo the plugin may report
 * like a change of its own. The editor window turns the former into undo
 * steps and tells the two apart by these.
 */
class HostedEffect final : public Effect,
                           private juce::AudioProcessorListener,
                           private juce::AsyncUpdater
{
public:
    enum class Status
    {
        loaded,              ///< the plugin runs
        loading,             ///< placeholder: the plugin is being created (asynchronously, in a process of its own); passes through meanwhile
        notLoaded,           ///< placeholder: no instance (getError says why)
        incompatibleLayout   ///< loaded, but the plugin refuses the chain's channel count: passes through
    };

    /// `instance` may be nullptr: a placeholder with `error` as the reason.
    HostedEffect (EffectTypeInfo type, juce::PluginDescription description,
                  std::unique_ptr<juce::AudioPluginInstance> instance, juce::String error = {});
    /// A placeholder waiting for its plugin: adopt() brings it.
    struct Loading {};
    HostedEffect (EffectTypeInfo type, juce::PluginDescription description, Loading);
    ~HostedEffect() override;

    /**
     * Message thread: the plugin a loading placeholder waited for, or nullptr
     * with the reason it did not come. The state the placeholder kept goes
     * into the plugin; the slot prepares it before the next plan runs it
     * (needsPrepare), and the chain hears of it through onLatencyChanged.
     */
    void adopt (std::unique_ptr<juce::AudioPluginInstance> instance, juce::String error);
    /// Message thread: a placeholder without a plugin starts waiting for one; adopt() brings it (or the reason).
    void expectPlugin();
    /// Expires with the effect: a posted callback checks it before touching the effect.
    std::weak_ptr<const int> getAliveToken() const noexcept { return aliveToken; }

    /// The count of its registry's effects that are still arriving (EffectRegistry::getPendingArrivals):
    /// a plugin requested for this effect, now or later, is counted there. None for an effect made outside a registry.
    void setArrivalCount (std::shared_ptr<std::atomic<int>> count) { arrivalCount = std::move (count); }
    const std::shared_ptr<std::atomic<int>>& getArrivalCount() const noexcept { return arrivalCount; }

    const juce::PluginDescription& getDescription() const noexcept { return description; }
    juce::AudioPluginInstance* getInstance() const noexcept { return instance.get(); }
    /// Whether the plugin is there (running or refusing the layout); false for a placeholder of either kind.
    bool isLoaded() const noexcept { return instance != nullptr; }
    Status getStatus() const noexcept { return status.load (std::memory_order_relaxed); }
    const juce::String& getError() const noexcept { return error; }
    bool needsPrepare() const noexcept override { return prepareNeeded; }

    /// Moves whenever the plugin reports a change of a parameter or of its state (any thread).
    juce::uint32 getPluginChangeCount() const noexcept { return pluginChanges.load (std::memory_order_relaxed); }
    /// Whether the plugin reports a control as held (between a gesture's begin and end).
    bool isGestureOpen() const noexcept { return openGestures.load (std::memory_order_relaxed) > 0; }
    /// Message thread: moves whenever the host put a parameter value or a state into the plugin.
    juce::uint32 getHostEditCount() const noexcept { return hostEdits; }

    void prepare (double sampleRate, int maxBlockSize, int numChannels) override;
    void release() override;
    void reset() noexcept override;
    void process (juce::dsp::AudioBlock<float>& block) noexcept override;
    int getLatencySamples() const noexcept override;
    double getTailSeconds() const noexcept override;

    std::span<const EffectParameterInfo> getParameters() const noexcept override { return infos; }
    int getNumParameters() const noexcept override { return static_cast<int> (infos.size()); }
    int getParameterIndex (std::string_view id) const noexcept override;
    float getParameter (int index) const noexcept override;
    float clampParameter (int index, float value) const noexcept override;
    void setParameter (int index, float value) noexcept override;
    juce::String getParameterText (int index, float value) const override;
    bool parseParameterText (int index, const juce::String& text, float& value) const override;

    json getState() const override;
    void setState (const json& state) override;
    json getPluginInfo() const override;

private:
    void buildParameterTable();
    bool negotiateLayout (int numChannels);
    juce::AudioProcessorParameter* parameterAt (int index) const noexcept;
    static std::string blobOf (const json& state);

    void takeInstance (std::unique_ptr<juce::AudioPluginInstance> created);

    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override;
    void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int) override;
    void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int) override;
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details) override;
    void handleAsyncUpdate() override;   // message thread: a latency change the plugin reported

    juce::PluginDescription description;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    std::atomic<Status> status;   // read on the render thread, written on the message thread
    juce::String error;
    bool prepareNeeded = false;   // message thread: the plugin arrived into a slot prepared without it

    // the parameter table: the infos point into these, which live as long as the effect
    std::vector<std::string> ids, names, units;
    std::vector<EffectParameterInfo> infos;
    std::vector<juce::AudioProcessorParameter*> parameters;

    // the state chunk as last read or set, and whether the plugin changed since
    mutable std::string cachedBlob;
    mutable std::atomic<bool> stateDirty { true };
    std::string keptBlob;   // a placeholder keeps what it was given

    // who changed the plugin: itself (its window, its own reports) or the host
    std::atomic<juce::uint32> pluginChanges { 0 };
    std::atomic<int> openGestures { 0 };
    juce::uint32 hostEdits = 0;

    // the render path: the chain's block wrapped for processBlock, no allocation
    juce::AudioBuffer<float> wrapper;
    juce::AudioBuffer<float> scratch;   // the stereo pair of a folded mono chain
    std::vector<float*> pointers;
    juce::MidiBuffer midi;
    int preparedChannels = 0;
    int preparedBlockSize = 0;
    bool foldMono = false;   // a mono chain run through a stereo-only plugin

    std::shared_ptr<std::atomic<int>> arrivalCount;
    std::shared_ptr<const int> aliveToken = std::make_shared<const int> (0);   // a creation callback in flight checks the effect still lives

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostedEffect)
};

} // namespace audium
