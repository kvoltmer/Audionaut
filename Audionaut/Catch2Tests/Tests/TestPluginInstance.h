//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

/// What the test plugin is and does; outside the class so a constructor can default to it.
struct TestPluginOptions
{
    juce::String name = "Test Gain";
    juce::String manufacturer = "Audionaut Tests";
    juce::String format = "TestA";
    int uid = 0x1234;
    bool stereoOnly = false;   ///< refuses every layout but stereo
    int maxChannels = 8;       ///< refuses more channels than this
    double tail = 0.0;
};

/// A hosted parameter of the test plugin: a range, a default, the plugin's own text.
class TestParameter final : public juce::AudioPluginInstance::HostedParameter
{
public:
    TestParameter (juce::String id_, juce::String name_, float min_, float max_, float default_, bool boolean_ = false) :
        id (std::move (id_)), name (std::move (name_)), min (min_), max (max_), def (default_), boolean (boolean_),
        normalised ((default_ - min_) / (max_ - min_))
    {
    }

    /// The value in its own units, as the plugin reads it.
    float get() const noexcept { return min + normalised.load() * (max - min); }

    juce::String getParameterID() const override { return id; }
    float getValue() const override { return normalised.load(); }
    void setValue (float newValue) override { normalised.store (boolean ? (newValue >= 0.5f ? 1.0f : 0.0f) : juce::jlimit (0.0f, 1.0f, newValue)); }
    float getDefaultValue() const override { return (def - min) / (max - min); }
    juce::String getName (int maximumStringLength) const override { return name.substring (0, maximumStringLength); }
    juce::String getLabel() const override { return boolean ? juce::String() : "x"; }
    bool isBoolean() const override { return boolean; }
    juce::String getText (float value, int) const override { return juce::String (min + value * (max - min), 3); }
    float getValueForText (const juce::String& text) const override { return (text.getFloatValue() - min) / (max - min); }

private:
    juce::String id, name;
    float min, max, def;
    bool boolean;
    std::atomic<float> normalised;
};

/**
 * A plugin for the hosting tests, in-process: a gain with a mute, a state
 * chunk of its own, a latency it can be told to report, and a channel
 * layout rule. What a real plugin would do through its format, this does
 * from a constructor, so the hosted effect and the host are tested without
 * a plugin installed.
 */
class TestPluginInstance final : public juce::AudioPluginInstance
{
public:
    using Options = TestPluginOptions;

    explicit TestPluginInstance (Options options_ = {}) :
        AudioPluginInstance (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo())
                                              .withOutput ("Output", juce::AudioChannelSet::stereo())),
        options (std::move (options_))
    {
        auto gainParameter = std::make_unique<TestParameter> ("gain", "Gain", 0.0f, 2.0f, 1.0f);
        auto muteParameter = std::make_unique<TestParameter> ("mute", "Mute", 0.0f, 1.0f, 0.0f, true);
        gain = gainParameter.get();
        mute = muteParameter.get();
        addHostedParameter (std::move (gainParameter));
        addHostedParameter (std::move (muteParameter));
        ++instances();
    }

    ~TestPluginInstance() override { --instances(); }

    /// How many are alive: a test that must not load a plugin checks this stays put.
    static int& instances() { static int count = 0; return count; }

    /// The description the format would give for these options.
    static juce::PluginDescription describe (const Options& options)
    {
        juce::PluginDescription description;
        description.name = options.name;
        description.descriptiveName = options.name;
        description.manufacturerName = options.manufacturer;
        description.pluginFormatName = options.format;
        description.fileOrIdentifier = "/test/" + options.name.replaceCharacter (' ', '_') + "." + options.format.toLowerCase();
        description.version = "1.0";
        description.uniqueId = options.uid;
        description.numInputChannels = 2;
        description.numOutputChannels = 2;
        return description;
    }

    void fillInPluginDescription (juce::PluginDescription& description) const override { description = describe (options); }

    //==============================================================================
    const juce::String getName() const override { return options.name; }
    void prepareToPlay (double, int) override { ++prepareCount; }
    void releaseResources() override { ++releaseCount; }
    double getTailLengthSeconds() const override { return options.tail; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        const auto in = layouts.getMainInputChannelSet();
        const auto out = layouts.getMainOutputChannelSet();
        if (in != out || in.isDisabled())
            return false;
        if (options.stereoOnly)
            return in == juce::AudioChannelSet::stereo();
        return in.size() >= 1 && in.size() <= options.maxChannels;
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        buffer.applyGain (mute->get() >= 0.5f ? 0.0f : gain->get());
        ++processCount;
        lastNumChannels = buffer.getNumChannels();
    }

    /// "gain|mute|generation": deterministic for a given state.
    void getStateInformation (juce::MemoryBlock& destination) override
    {
        const auto text = juce::String (gain->get(), 6) + "|" + (mute->get() >= 0.5f ? "1" : "0") + "|" + juce::String (generation);
        destination.append (text.toRawUTF8(), text.getNumBytesAsUTF8());
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        ++setStateCount;
        const auto text = juce::String::fromUTF8 (static_cast<const char*> (data), sizeInBytes);
        auto parts = juce::StringArray::fromTokens (text, "|", "");
        if (parts.size() < 3)
            return;
        gain->setValueNotifyingHost (parts[0].getFloatValue() / 2.0f);
        mute->setValueNotifyingHost (parts[1] == "1" ? 1.0f : 0.0f);
        generation = parts[2].getIntValue();
    }

    /// A plugin that moves its latency of its own accord: fires audioProcessorChanged (latencyChanged).
    void setReportedLatency (int samples) { setLatencySamples (samples); }

    /// Something in the state that is not a parameter changed (a preset name, say).
    void bumpGeneration() { ++generation; updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true)); }

    Options options;
    TestParameter* gain = nullptr;
    TestParameter* mute = nullptr;
    int generation = 0;
    int prepareCount = 0, releaseCount = 0, processCount = 0, setStateCount = 0;
    int lastNumChannels = 0;
};

/**
 * A plugin format the tests give a PluginHost: it "finds" the plugins it
 * was told about and creates TestPluginInstances for them, in-process.
 */
class TestPluginFormat final : public juce::AudioPluginFormat
{
public:
    explicit TestPluginFormat (juce::String name_, std::vector<TestPluginInstance::Options> plugins_ = {}) :
        name (std::move (name_)), plugins (std::move (plugins_))
    {
        for (auto& plugin : plugins)
            plugin.format = name;
    }

    juce::String getName() const override { return name; }

    void findAllTypesForFile (juce::OwnedArray<juce::PluginDescription>& results, const juce::String& fileOrIdentifier) override
    {
        for (const auto& plugin : plugins)
            if (TestPluginInstance::describe (plugin).fileOrIdentifier == fileOrIdentifier)
                results.add (new juce::PluginDescription (TestPluginInstance::describe (plugin)));
    }

    bool fileMightContainThisPluginType (const juce::String& fileOrIdentifier) override { return fileOrIdentifier.startsWith ("/test/"); }
    juce::String getNameOfPluginFromIdentifier (const juce::String& fileOrIdentifier) override { return fileOrIdentifier; }
    bool pluginNeedsRescanning (const juce::PluginDescription&) override { return false; }
    bool doesPluginStillExist (const juce::PluginDescription&) override { return true; }
    bool canScanForPlugins() const override { return true; }
    bool isTrivialToScan() const override { return true; }

    juce::StringArray searchPathsForPlugins (const juce::FileSearchPath&, bool, bool) override
    {
        juce::StringArray files;
        for (const auto& plugin : plugins)
            files.add (TestPluginInstance::describe (plugin).fileOrIdentifier);
        return files;
    }

    juce::FileSearchPath getDefaultLocationsToSearch() override { return {}; }
    bool requiresUnblockedMessageThreadDuringCreation (const juce::PluginDescription&) const override { return asyncOnly; }

    bool asyncOnly = false;   ///< behaves like AUv3: refuses synchronous creation

private:
    void createPluginInstance (const juce::PluginDescription& description, double, int, PluginCreationCallback callback) override
    {
        for (const auto& plugin : plugins)
        {
            if (TestPluginInstance::describe (plugin).uniqueId == description.uniqueId)
            {
                callback (std::make_unique<TestPluginInstance> (plugin), {});
                return;
            }
        }
        callback (nullptr, "no such test plugin");
    }

    juce::String name;
    std::vector<TestPluginInstance::Options> plugins;
};
