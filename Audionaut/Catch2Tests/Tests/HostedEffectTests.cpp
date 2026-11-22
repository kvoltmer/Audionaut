//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

#include "Engine/Effects/EffectRegistry.h"
#include "Engine/Effects/EffectSlot.h"
#include "Engine/Effects/Hosting/HostedEffect.h"
#include "Engine/Effects/Hosting/PluginTypeId.h"
#include "Engine/Effects/InsertChain.h"
#include "Engine/Effects/MixPlanContainer.h"
#include "Engine/Effects/MixerPlanner.h"
#include "TestPluginInstance.h"

using namespace audium;
using Catch::Approx;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;

    /// A message thread for the duration of a scenario: the latency report is posted to it.
    struct MessageThread
    {
        MessageThread()
        {
            juce::MessageManager::getInstance();
            lock = std::make_unique<juce::MessageManagerLock> (juce::Thread::getCurrentThread());
        }
        ~MessageThread()
        {
            lock = nullptr;
            juce::DeletedAtShutdown::deleteAll();
            juce::MessageManager::deleteInstance();
        }
        std::unique_ptr<juce::MessageManagerLock> lock;
    };

    juce::AudioBuffer<float> makeDc (int numChannels, float level)
    {
        juce::AudioBuffer<float> buffer (numChannels, kBlockSize);
        for (auto c = 0; c < numChannels; ++c)
            juce::FloatVectorOperations::fill (buffer.getWritePointer (c), level, kBlockSize);
        return buffer;
    }

    void run (Effect& effect, juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        effect.process (block);
    }

    /// A hosted effect around a test plugin, the plugin still reachable.
    struct Hosted
    {
        explicit Hosted (TestPluginInstance::Options options = {})
        {
            auto instance = std::make_unique<TestPluginInstance> (options);
            plugin = instance.get();
            description = TestPluginInstance::describe (options);
            effect = std::make_unique<HostedEffect> (PluginTypeId::typeInfo (description), description, std::move (instance));
        }

        TestPluginInstance* plugin = nullptr;
        juce::PluginDescription description;
        std::unique_ptr<HostedEffect> effect;
    };
}

SCENARIO ("a plugin's type id and plugin block name it across machines", "[engine][effects][hosting]")
{
    GIVEN ("descriptions of the three formats")
    {
        TestPluginInstance::Options options;
        options.uid = 0x1a2b3c4d;

        THEN ("the id is the format's key, a colon, the unique id as eight hex digits")
        {
            options.format = "AudioUnit";
            REQUIRE (PluginTypeId::make (TestPluginInstance::describe (options)) == "au:1a2b3c4d");
            options.format = "VST3";
            REQUIRE (PluginTypeId::make (TestPluginInstance::describe (options)) == "vst3:1a2b3c4d");
            options.format = "LV2";
            REQUIRE (PluginTypeId::make (TestPluginInstance::describe (options)) == "lv2:1a2b3c4d");
        }

        THEN ("a small id is zero-padded, a negative one is its two's complement, and the deprecated uid stands in for none")
        {
            options.format = "VST3";
            options.uid = 0x2a;
            REQUIRE (PluginTypeId::make (TestPluginInstance::describe (options)) == "vst3:0000002a");

            auto description = TestPluginInstance::describe (options);
            description.uniqueId = -1;
            REQUIRE (PluginTypeId::make (description) == "vst3:ffffffff");

            description.uniqueId = 0;
            description.deprecatedUid = 0x99;
            REQUIRE (PluginTypeId::make (description) == "vst3:00000099");
        }

        THEN ("the plugin block carries name, manufacturer, format, file and version")
        {
            const auto info = PluginTypeId::pluginInfo (TestPluginInstance::describe (options));
            REQUIRE (info["name"] == "Test Gain");
            REQUIRE (info["manufacturer"] == "Audionaut Tests");
            REQUIRE (info["format"] == "TestA");
            REQUIRE (info["file"].get<std::string>().find ("/test/") == 0);
            REQUIRE (info["version"] == "1.0");
        }

        THEN ("the type info lists the plugin under its manufacturer, the format in the description")
        {
            const auto type = PluginTypeId::typeInfo (TestPluginInstance::describe (options));
            REQUIRE (type.name == "Test Gain");
            REQUIRE (type.category == "Audionaut Tests");
            REQUIRE (type.description == "Test Gain (TestA)");
        }
    }
}

SCENARIO ("a hosted effect speaks for the plugin's parameters", "[engine][effects][hosting]")
{
    GIVEN ("a hosted test plugin with a gain and a mute")
    {
        Hosted hosted;
        auto& effect = *hosted.effect;

        THEN ("the parameters are the plugin's, under its ids, normalised 0..1")
        {
            REQUIRE (effect.getNumParameters() == 2);
            REQUIRE (effect.getParameterIndex ("gain") == 0);
            REQUIRE (effect.getParameterIndex ("mute") == 1);
            REQUIRE (effect.getParameterIndex ("nope") == -1);

            const auto parameters = effect.getParameters();
            REQUIRE (std::string (parameters[0].name) == "Gain");
            REQUIRE (parameters[0].min == 0.0f);
            REQUIRE (parameters[0].max == 1.0f);
            REQUIRE (parameters[0].def == Approx (0.5f));   // gain 1 of 0..2
            REQUIRE_FALSE (parameters[0].boolean);
            REQUIRE (parameters[1].boolean);
        }

        THEN ("a value set is the plugin's value, clamped, and the text is the plugin's own")
        {
            effect.setParameter (0, 0.75f);
            REQUIRE (effect.getParameter (0) == Approx (0.75f));
            REQUIRE (hosted.plugin->gain->get() == Approx (1.5f));

            effect.setParameter (0, 7.0f);
            REQUIRE (effect.getParameter (0) == Approx (1.0f));
            REQUIRE (effect.clampParameter (0, -3.0f) == 0.0f);
            REQUIRE (effect.clampParameter (1, 0.7f) == 1.0f);
            REQUIRE (effect.clampParameter (1, 0.2f) == 0.0f);

            REQUIRE (effect.getParameterText (0, 0.5f).isNotEmpty());
            float value = -1.0f;
            REQUIRE (effect.parseParameterText (0, effect.getParameterText (0, 0.25f), value));
            REQUIRE (value == Approx (0.25f).margin (0.01f));

            effect.setParameter (1, 1.0f);
            REQUIRE (hosted.plugin->mute->get() == 1.0f);
        }

        THEN ("the plugin block is the one the project writes")
        {
            REQUIRE (effect.getPluginInfo()["manufacturer"] == "Audionaut Tests");
        }
    }
}

SCENARIO ("a hosted effect runs the plugin on the chain's channels", "[engine][effects][hosting]")
{
    GIVEN ("a plugin that takes one to eight channels")
    {
        Hosted hosted;
        auto& effect = *hosted.effect;

        WHEN ("prepared for two channels at half gain")
        {
            effect.prepare (kSampleRate, kBlockSize, 2);
            effect.setParameter (0, 0.25f);   // gain 0.5
            auto buffer = makeDc (2, 0.8f);
            run (effect, buffer);

            THEN ("the plugin saw two channels and halved them")
            {
                REQUIRE (effect.getStatus() == HostedEffect::Status::loaded);
                REQUIRE (hosted.plugin->prepareCount == 1);
                REQUIRE (hosted.plugin->lastNumChannels == 2);
                REQUIRE (buffer.getSample (0, 10) == Approx (0.4f));
                REQUIRE (buffer.getSample (1, 10) == Approx (0.4f));
            }
        }

        WHEN ("prepared for one channel")
        {
            effect.prepare (kSampleRate, kBlockSize, 1);
            auto buffer = makeDc (1, 0.8f);
            run (effect, buffer);

            THEN ("the plugin runs in mono")
            {
                REQUIRE (effect.getStatus() == HostedEffect::Status::loaded);
                REQUIRE (hosted.plugin->lastNumChannels == 1);
                REQUIRE (buffer.getSample (0, 10) == Approx (0.8f));
            }
        }

        WHEN ("prepared again for another channel count")
        {
            effect.prepare (kSampleRate, kBlockSize, 2);
            effect.prepare (kSampleRate, kBlockSize, 4);

            THEN ("the plugin let go of the first format before taking the second")
            {
                REQUIRE (hosted.plugin->releaseCount == 1);
                REQUIRE (hosted.plugin->prepareCount == 2);
                auto buffer = makeDc (4, 0.5f);
                run (effect, buffer);
                REQUIRE (hosted.plugin->lastNumChannels == 4);
            }
        }

        WHEN ("a block with the wrong channel count arrives")
        {
            effect.prepare (kSampleRate, kBlockSize, 2);
            auto buffer = makeDc (3, 0.5f);
            run (effect, buffer);

            THEN ("it passes untouched")
            {
                REQUIRE (hosted.plugin->processCount == 0);
                REQUIRE (buffer.getSample (2, 5) == Approx (0.5f));
            }
        }
    }

    GIVEN ("a plugin that refuses more than two channels")
    {
        TestPluginInstance::Options options;
        options.maxChannels = 2;
        Hosted hosted (options);
        auto& effect = *hosted.effect;

        WHEN ("prepared for four channels")
        {
            effect.prepare (kSampleRate, kBlockSize, 4);
            effect.setParameter (1, 1.0f);   // mute: would silence everything if it ran
            auto buffer = makeDc (4, 0.5f);
            run (effect, buffer);

            THEN ("the slot passes the signal through and says why")
            {
                REQUIRE (effect.getStatus() == HostedEffect::Status::incompatibleLayout);
                REQUIRE (effect.getError().contains ("4 channels"));
                REQUIRE (hosted.plugin->processCount == 0);
                REQUIRE (buffer.getSample (3, 5) == Approx (0.5f));
                REQUIRE (effect.getLatencySamples() == 0);
            }
        }
    }

    GIVEN ("a plugin that only runs in stereo")
    {
        TestPluginInstance::Options options;
        options.stereoOnly = true;
        Hosted hosted (options);
        auto& effect = *hosted.effect;

        WHEN ("prepared for a mono chain at half gain")
        {
            effect.prepare (kSampleRate, kBlockSize, 1);
            effect.setParameter (0, 0.25f);
            auto buffer = makeDc (1, 0.8f);
            run (effect, buffer);

            THEN ("the plugin runs in stereo on the one channel, and the sides come back as one")
            {
                REQUIRE (effect.getStatus() == HostedEffect::Status::loaded);
                REQUIRE (hosted.plugin->lastNumChannels == 2);
                REQUIRE (buffer.getSample (0, 10) == Approx (0.4f));
            }
        }

        WHEN ("prepared for four channels")
        {
            effect.prepare (kSampleRate, kBlockSize, 4);

            THEN ("it is incompatible: no folding for anything but mono")
            {
                REQUIRE (effect.getStatus() == HostedEffect::Status::incompatibleLayout);
            }
        }
    }
}

SCENARIO ("a plugin with a bus it will not switch off runs with silence on that bus", "[engine][effects][hosting]")
{
    GIVEN ("a plugin that insists on its stereo sidechain input")
    {
        TestPluginInstance::Options options;
        options.fixedSidechain = true;
        Hosted hosted (options);
        auto& effect = *hosted.effect;

        WHEN ("prepared for a stereo chain and run twice at half gain")
        {
            effect.prepare (kSampleRate, kBlockSize, 2);
            effect.setParameter (0, 0.25f);   // gain 0.5
            auto first = makeDc (2, 0.8f);
            run (effect, first);
            auto second = makeDc (2, 0.8f);
            run (effect, second);

            THEN ("the plugin gets the chain's pair and a silent sidechain, and the chain gets the main bus back")
            {
                REQUIRE (effect.getStatus() == HostedEffect::Status::loaded);
                REQUIRE (effect.getError().isEmpty());
                REQUIRE (hosted.plugin->processCount == 2);
                REQUIRE (hosted.plugin->lastNumChannels == 4);
                REQUIRE (hosted.plugin->sidechainPeak == 0.0f);
                REQUIRE (second.getSample (0, 10) == Approx (0.4f));
                REQUIRE (second.getSample (1, 10) == Approx (0.4f));
            }
        }

        WHEN ("prepared for a mono chain")
        {
            options.stereoOnly = true;
            Hosted stereoOnly (options);
            stereoOnly.effect->prepare (kSampleRate, kBlockSize, 1);
            stereoOnly.effect->setParameter (0, 0.25f);
            auto buffer = makeDc (1, 0.8f);
            run (*stereoOnly.effect, buffer);

            THEN ("the one channel goes through the plugin's pair, the sidechain silent beside it")
            {
                REQUIRE (stereoOnly.effect->getStatus() == HostedEffect::Status::loaded);
                REQUIRE (stereoOnly.plugin->lastNumChannels == 4);
                REQUIRE (stereoOnly.plugin->sidechainPeak == 0.0f);
                REQUIRE (buffer.getSample (0, 10) == Approx (0.4f));
            }
        }
    }
}

SCENARIO ("a hosted effect tells the plugin's own changes from the host's", "[engine][effects][hosting]")
{
    GIVEN ("a hosted test plugin")
    {
        Hosted hosted;
        auto& effect = *hosted.effect;
        const auto pluginChanges = effect.getPluginChangeCount();
        const auto hostEdits = effect.getHostEditCount();

        WHEN ("the host sets a parameter and a state")
        {
            effect.setParameter (0, 0.75f);
            const auto afterParameter = effect.getHostEditCount();
            effect.setState (json { { "blob", juce::MemoryBlock ("0.500000|0|3", 12).toBase64Encoding().toStdString() } });

            THEN ("each counts as a host edit")
            {
                REQUIRE (afterParameter != hostEdits);
                REQUIRE (effect.getHostEditCount() != afterParameter);
            }

            AND_WHEN ("the same state is set again")
            {
                const auto before = effect.getHostEditCount();
                effect.setState (effect.getState());

                THEN ("nothing was put into the plugin, and nothing is counted")
                {
                    REQUIRE (effect.getHostEditCount() == before);
                }
            }
        }

        WHEN ("the plugin moves a control of its own, as its window does")
        {
            hosted.plugin->gain->beginChangeGesture();
            hosted.plugin->gain->setValueNotifyingHost (0.1f);

            THEN ("the change is counted as the plugin's, with the control held")
            {
                REQUIRE (effect.getPluginChangeCount() != pluginChanges);
                REQUIRE (effect.getHostEditCount() == hostEdits);
                REQUIRE (effect.isGestureOpen());

                hosted.plugin->gain->endChangeGesture();
                REQUIRE_FALSE (effect.isGestureOpen());
            }
        }

        WHEN ("the plugin reports a state change that is no parameter")
        {
            hosted.plugin->bumpGeneration();

            THEN ("that counts too")
            {
                REQUIRE (effect.getPluginChangeCount() != pluginChanges);
            }
        }
    }
}

SCENARIO ("a hosted effect's state is the plugin's chunk, read when it changed", "[engine][effects][hosting]")
{
    GIVEN ("a hosted test plugin")
    {
        Hosted hosted;
        auto& effect = *hosted.effect;

        THEN ("the state is a blob, the same twice over, and another after a parameter moved")
        {
            const auto first = effect.getState();
            REQUIRE (first.is_object());
            REQUIRE (first["blob"].is_string());
            REQUIRE_FALSE (first["blob"].get<std::string>().empty());
            REQUIRE (effect.getState() == first);

            effect.setParameter (0, 0.75f);
            const auto second = effect.getState();
            REQUIRE (second != first);

            juce::MemoryBlock chunk;
            REQUIRE (chunk.fromBase64Encoding (second["blob"].get<std::string>()));
            REQUIRE (chunk.toString().startsWith ("1.5"));
        }

        THEN ("a change the plugin reports on its own is read too")
        {
            const auto before = effect.getState();
            hosted.plugin->bumpGeneration();
            REQUIRE (effect.getState() != before);
        }

        THEN ("setting a state hands the chunk to the plugin, and the same chunk again is not handed over twice")
        {
            effect.setParameter (0, 0.75f);
            const auto saved = effect.getState();
            effect.setParameter (0, 0.1f);
            REQUIRE (hosted.plugin->gain->get() == Approx (0.2f));

            effect.setState (saved);
            REQUIRE (hosted.plugin->setStateCount == 1);
            REQUIRE (hosted.plugin->gain->get() == Approx (1.5f));
            REQUIRE (effect.getState() == saved);

            effect.setState (saved);   // an undo replay reads every element again
            REQUIRE (hosted.plugin->setStateCount == 1);

            effect.setState (saved["blob"]);   // a bare string is a blob too
            REQUIRE (hosted.plugin->setStateCount == 1);
        }

        THEN ("a built-in's state (a type swapped in a project) leaves the plugin alone")
        {
            effect.setState ({ { "gain_db", -6.0 } });
            REQUIRE (hosted.plugin->setStateCount == 0);
        }
    }

    GIVEN ("a placeholder: the plugin could not be loaded")
    {
        auto description = TestPluginInstance::describe ({});
        HostedEffect placeholder (PluginTypeId::typeInfo (description), description, nullptr, "not installed");

        THEN ("it is not loaded, has no parameters and passes the signal through")
        {
            REQUIRE_FALSE (placeholder.isLoaded());
            REQUIRE (placeholder.getStatus() == HostedEffect::Status::notLoaded);
            REQUIRE (placeholder.getError() == "not installed");
            REQUIRE (placeholder.getNumParameters() == 0);
            placeholder.prepare (kSampleRate, kBlockSize, 2);
            auto buffer = makeDc (2, 0.5f);
            run (placeholder, buffer);
            REQUIRE (buffer.getSample (1, 5) == Approx (0.5f));
            REQUIRE (placeholder.getLatencySamples() == 0);
        }

        THEN ("it keeps the blob it is given and writes it back, so the project loses nothing")
        {
            placeholder.setState ({ { "blob", "QUJD" } });
            REQUIRE (placeholder.getState()["blob"] == "QUJD");
            REQUIRE (placeholder.getPluginInfo()["name"] == "Test Gain");
        }
    }

    GIVEN ("a placeholder waiting for its plugin, in a prepared slot")
    {
        MessageThread messageThread;
        auto description = TestPluginInstance::describe ({});
        auto loading = std::make_unique<HostedEffect> (PluginTypeId::typeInfo (description), description, HostedEffect::Loading {});
        auto* effect = loading.get();
        auto latencyReports = 0;
        effect->onLatencyChanged = [&latencyReports] { ++latencyReports; };
        EffectSlot slot (std::move (loading));
        slot.prepare (kSampleRate, kBlockSize, 2);

        THEN ("it is loading, not loaded, and passes the signal through")
        {
            REQUIRE (effect->getStatus() == HostedEffect::Status::loading);
            REQUIRE_FALSE (effect->isLoaded());
            REQUIRE (effect->getNumParameters() == 0);
            REQUIRE_FALSE (effect->needsPrepare());
            REQUIRE (slot.isPreparedFor (kSampleRate, kBlockSize, 2));
            auto buffer = makeDc (2, 0.5f);
            run (*effect, buffer);
            REQUIRE (buffer.getSample (1, 5) == Approx (0.5f));
        }

        WHEN ("the plugin arrives, with a state the placeholder kept")
        {
            effect->setState ({ { "blob", juce::MemoryBlock ("1.500000|0|0", 12).toBase64Encoding().toStdString() } });
            auto instance = std::make_unique<TestPluginInstance> (TestPluginInstance::Options {});
            auto* plugin = instance.get();
            effect->adopt (std::move (instance), {});

            THEN ("it is loaded with the kept state, and the slot has to prepare it before it runs")
            {
                REQUIRE (effect->getStatus() == HostedEffect::Status::loaded);
                REQUIRE (effect->isLoaded());
                REQUIRE (effect->getNumParameters() == 2);
                REQUIRE (plugin->setStateCount == 1);
                REQUIRE (plugin->gain->get() == Approx (1.5f));
                REQUIRE (latencyReports == 1);

                REQUIRE (effect->needsPrepare());
                REQUIRE_FALSE (slot.isPreparedFor (kSampleRate, kBlockSize, 2));
                auto buffer = makeDc (2, 0.5f);
                run (*effect, buffer);
                REQUIRE (buffer.getSample (1, 5) == Approx (0.5f));   // unprepared: still passing through
                REQUIRE (plugin->processCount == 0);

                slot.prepare (kSampleRate, kBlockSize, 2);
                REQUIRE_FALSE (effect->needsPrepare());
                REQUIRE (slot.isPreparedFor (kSampleRate, kBlockSize, 2));
                REQUIRE (plugin->prepareCount == 1);
                run (*effect, buffer);
                REQUIRE (plugin->processCount == 1);
                REQUIRE (buffer.getSample (1, 5) == Approx (0.75f));   // gain 1.5
            }
        }

        WHEN ("the plugin does not come")
        {
            effect->adopt (nullptr, "the hosting service refused it");

            THEN ("it is a plain placeholder with the reason")
            {
                REQUIRE (effect->getStatus() == HostedEffect::Status::notLoaded);
                REQUIRE_FALSE (effect->isLoaded());
                REQUIRE (effect->getError() == "the hosting service refused it");
                REQUIRE (latencyReports == 0);
            }
        }
    }
}

SCENARIO ("a plugin that moves its own latency gets the chain a new plan", "[engine][effects][hosting][latency]")
{
    MessageThread messageThread;

    GIVEN ("a hosted plugin in a slot of a chain with a planner")
    {
        auto registry = std::make_shared<EffectRegistry>();
        TestPluginInstance* plugin = nullptr;
        auto description = TestPluginInstance::describe ({});
        registry->add (PluginTypeId::typeInfo (description), [&]
        {
            auto instance = std::make_unique<TestPluginInstance>();
            plugin = instance.get();
            return std::make_unique<HostedEffect> (PluginTypeId::typeInfo (description), description, std::move (instance));
        });
        MixerPlanner planner (registry, std::make_shared<MixPlanContainer>());
        InsertChain chain (&planner);
        auto slot = chain.add (PluginTypeId::make (description));
        REQUIRE (slot != nullptr);
        REQUIRE (plugin != nullptr);
        slot->prepare (kSampleRate, kBlockSize, 2);
        REQUIRE (slot->getLatencySamples() == 0);

        WHEN ("the plugin reports 128 samples of latency")
        {
            auto rebuildsBefore = 0;
            auto heard = false;
            slot->getEffect().onLatencyChanged = [&] { heard = true; };   // in place of the chain's: a planner without a container rebuilds nothing
            plugin->setReportedLatency (128);

            THEN ("the slot is stale and the chain hears of it on the message thread")
            {
                REQUIRE (slot->hasStaleLatency());
                REQUIRE (slot->getEffect().getLatencySamples() == 128);
                for (auto tries = 0; tries < 50 && ! heard; ++tries)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
                REQUIRE (heard);
                juce::ignoreUnused (rebuildsBefore);
            }
        }

        WHEN ("the slot leaves the chain before the report arrives")
        {
            plugin->setReportedLatency (64);
            REQUIRE (chain.remove (0));
            slot.reset();   // the effect and the plugin go with the last reference

            THEN ("the posted report finds nobody and nothing breaks")
            {
                for (auto tries = 0; tries < 5; ++tries)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
                REQUIRE (chain.isEmpty());
            }
        }
    }
}

SCENARIO ("a hosted effect tells the plugin where the transport is", "[engine][effects][hosting][playhead]")
{
    GIVEN ("a plugin whose effect was given the engine's render transport")
    {
        Hosted hosted;
        auto& effect = *hosted.effect;
        auto transport = std::make_shared<RenderTransport>();
        effect.setRenderTransport (transport);
        effect.prepare (kSampleRate, kBlockSize, 2);

        WHEN ("a block is rendered while the transport plays inside a loop")
        {
            RenderTransport::Block block;
            block.playing = true;
            block.positionSamples = 96000;
            block.positionSeconds = 2.0;
            block.positionBeats = 4.0;
            block.tempo = 120.0;
            block.looping = true;
            block.loopStartBeats = 2.0;
            block.loopEndBeats = 6.0;
            transport->publish (block);

            auto buffer = makeDc (2, 0.5f);
            run (effect, buffer);

            THEN ("the plugin's playhead says so")
            {
                REQUIRE (hosted.plugin->positions.size() == 1);
                const auto& position = hosted.plugin->positions.back();
                REQUIRE (position.getIsPlaying());
                REQUIRE (position.getTimeInSamples() == juce::Optional<juce::int64> (96000));
                REQUIRE (*position.getTimeInSeconds() == Approx (2.0));
                REQUIRE (*position.getPpqPosition() == Approx (4.0));
                REQUIRE (*position.getBpm() == Approx (120.0));
                REQUIRE (position.getIsLooping());
                REQUIRE (position.getLoopPoints().hasValue());
                REQUIRE (position.getLoopPoints()->ppqStart == Approx (2.0));
                REQUIRE (position.getLoopPoints()->ppqEnd == Approx (6.0));
            }

            THEN ("what the project does not have is not made up")
            {
                const auto& position = hosted.plugin->positions.back();
                REQUIRE_FALSE (position.getTimeSignature().hasValue());
                REQUIRE_FALSE (position.getPpqPositionOfLastBarStart().hasValue());
                REQUIRE_FALSE (position.getIsRecording());
            }
        }

        WHEN ("a block is rendered while the transport stands")
        {
            RenderTransport::Block block;
            block.positionSamples = 48000;
            block.positionSeconds = 1.0;
            block.positionBeats = 1.5;
            block.tempo = 90.0;
            transport->publish (block);

            auto buffer = makeDc (2, 0.5f);
            run (effect, buffer);

            THEN ("the plugin hears of the position and the tempo, not playing and without a loop")
            {
                REQUIRE (hosted.plugin->positions.size() == 1);
                const auto& position = hosted.plugin->positions.back();
                REQUIRE_FALSE (position.getIsPlaying());
                REQUIRE (*position.getPpqPosition() == Approx (1.5));
                REQUIRE (*position.getBpm() == Approx (90.0));
                REQUIRE_FALSE (position.getIsLooping());
                REQUIRE_FALSE (position.getLoopPoints().hasValue());
            }
        }
    }

    GIVEN ("a plugin whose effect has no render transport")
    {
        Hosted hosted;
        hosted.effect->prepare (kSampleRate, kBlockSize, 2);
        auto buffer = makeDc (2, 0.5f);
        run (*hosted.effect, buffer);

        THEN ("the plugin has no playhead to ask")
        {
            REQUIRE (hosted.plugin->processCount == 1);
            REQUIRE (hosted.plugin->getPlayHead() == nullptr);
            REQUIRE (hosted.plugin->positions.empty());
        }
    }

    GIVEN ("a loading placeholder that was given the render transport")
    {
        MessageThread messageThread;
        const auto description = TestPluginInstance::describe ({});
        HostedEffect effect (PluginTypeId::typeInfo (description), description, HostedEffect::Loading {});
        effect.setRenderTransport (std::make_shared<RenderTransport>());

        WHEN ("its plugin arrives")
        {
            auto instance = std::make_unique<TestPluginInstance>();
            auto* plugin = instance.get();
            effect.adopt (std::move (instance), {});

            THEN ("the plugin gets the playhead too")
            {
                REQUIRE (plugin->getPlayHead() != nullptr);
            }
        }
    }
}

SCENARIO ("a hosted effect tells the plugin when the renders to come are a bounce's", "[engine][effects][hosting][offline]")
{
    GIVEN ("a plugin prepared for live rendering")
    {
        Hosted hosted;
        auto& effect = *hosted.effect;
        effect.prepare (kSampleRate, kBlockSize, 2);
        REQUIRE_FALSE (hosted.plugin->isNonRealtime());
        REQUIRE_FALSE (effect.needsPrepare());

        WHEN ("the effect hears that a bounce comes")
        {
            effect.setOfflineRendering (true);

            THEN ("the plugin has the flag at once, and needs no new prepare for it")
            {
                REQUIRE (hosted.plugin->isNonRealtime());
                REQUIRE_FALSE (effect.needsPrepare());
            }

            AND_WHEN ("it runs a block")
            {
                auto buffer = makeDc (2, 0.5f);
                run (effect, buffer);

                THEN ("the plugin ran in offline mode")
                {
                    REQUIRE (hosted.plugin->offlineBlocks == 1);
                    REQUIRE (hosted.plugin->liveBlocks == 0);
                }
            }

            AND_WHEN ("the bounce is over")
            {
                effect.setOfflineRendering (false);

                THEN ("the plugin is back to live")
                {
                    REQUIRE_FALSE (hosted.plugin->isNonRealtime());
                    REQUIRE_FALSE (effect.needsPrepare());
                    REQUIRE (hosted.plugin->prepareCount == 1);
                }
            }
        }
    }

    GIVEN ("a VST3 plugin prepared for live rendering")
    {
        TestPluginInstance::Options options;
        options.format = "VST3";
        Hosted hosted (options);
        auto& effect = *hosted.effect;
        effect.prepare (kSampleRate, kBlockSize, 2);

        WHEN ("the effect hears that a bounce comes")
        {
            effect.setOfflineRendering (true);

            THEN ("the effect asks to be prepared again: a VST3 reads the mode when it is set up")
            {
                REQUIRE (hosted.plugin->isNonRealtime());
                REQUIRE (effect.needsPrepare());
            }

            AND_WHEN ("it is prepared")
            {
                effect.prepare (kSampleRate, kBlockSize, 2);

                THEN ("the plugin was prepared in offline mode")
                {
                    REQUIRE_FALSE (effect.needsPrepare());
                    REQUIRE (hosted.plugin->preparedOffline);
                }

                AND_WHEN ("the bounce is over")
                {
                    effect.setOfflineRendering (false);

                    THEN ("it is to be prepared again, for live rendering")
                    {
                        REQUIRE_FALSE (hosted.plugin->isNonRealtime());
                        REQUIRE (effect.needsPrepare());
                    }
                }
            }
        }

        WHEN ("it is told live rendering again, which it is prepared for")
        {
            effect.setOfflineRendering (false);

            THEN ("nothing changes")
            {
                REQUIRE_FALSE (effect.needsPrepare());
                REQUIRE (hosted.plugin->prepareCount == 1);
            }
        }
    }

    GIVEN ("a loading placeholder that hears of a bounce")
    {
        MessageThread messageThread;
        const auto description = TestPluginInstance::describe ({});
        HostedEffect effect (PluginTypeId::typeInfo (description), description, HostedEffect::Loading {});
        effect.setOfflineRendering (true);

        WHEN ("its plugin arrives")
        {
            auto instance = std::make_unique<TestPluginInstance>();
            auto* plugin = instance.get();
            effect.adopt (std::move (instance), {});

            THEN ("the plugin is told at once")
            {
                REQUIRE (plugin->isNonRealtime());
            }
        }
    }
}
