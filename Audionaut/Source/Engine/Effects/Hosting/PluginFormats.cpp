//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "PluginFormats.h"

#include <atomic>
#include <memory>
#include <thread>

namespace audium
{

#if JUCE_MAC && JUCE_PLUGINHOST_AU
namespace
{
    /// The Audio Units a scan looks at: the effects (with the music effects, which take MIDI as
    /// well). Instruments, generators, MIDI processors, mixers and panners are not hosted, so they
    /// are not loaded to be listed either - the system says what a component is before it is opened.
    struct EffectAudioUnits final : public juce::AudioUnitPluginFormat
    {
        juce::StringArray searchPathsForPlugins (const juce::FileSearchPath& paths, bool recursive, bool allowAsync) override
        {
            juce::StringArray effects;
            for (const auto& identifier : juce::AudioUnitPluginFormat::searchPathsForPlugins (paths, recursive, allowAsync))
                if (identifier.startsWith ("AudioUnit:Effects/"))
                    effects.add (identifier);
            return effects;
        }
    };
}
#endif

void PluginFormats::addPlatformFormats (juce::AudioPluginFormatManager& manager)
{
   #if JUCE_MAC && JUCE_PLUGINHOST_AU
    // the sandbox's one format; see applyLoweredSecurity() for the plugins the sandbox does not open in-process
    manager.addFormat (std::make_unique<EffectAudioUnits>());
   #endif
   #if (JUCE_WINDOWS || JUCE_LINUX || JUCE_BSD) && JUCE_PLUGINHOST_VST3
    manager.addFormat (std::make_unique<juce::VST3PluginFormat>());
   #endif
   #if (JUCE_LINUX || JUCE_BSD) && JUCE_PLUGINHOST_LV2
    manager.addFormat (std::make_unique<juce::LV2PluginFormat>());
   #endif
    juce::ignoreUnused (manager);
}

void PluginFormats::applyLoweredSecurity (juce::AudioPluginFormatManager& manager, bool allowed)
{
   #if JUCE_MAC && JUCE_PLUGINHOST_AU
    // Inside the sandbox the OS refuses to open an AUv2 without the sandbox-safe flag in-process
    // (-3000). Allowed, those run in the system's hosting service, as they do in Logic - which the
    // system grants once the user agreed to lower the app's security settings. Not allowed, they are
    // asked for in-process and fail, and nobody is asked anything.
    using Isolation = juce::AudioUnitPluginFormat::ProcessIsolation;
    for (auto* format : manager.getFormats())
        if (auto* audioUnits = dynamic_cast<juce::AudioUnitPluginFormat*> (format))
            audioUnits->setProcessIsolation (allowed ? Isolation::whenSandboxRequires : Isolation::inProcess);
   #endif
    juce::ignoreUnused (manager, allowed);
}

bool PluginFormats::runWhilePumping (std::function<void()> fn, int timeoutMs)
{
    auto done = std::make_shared<std::atomic<bool>> (false);
    std::thread ([work = std::move (fn), done] { work(); done->store (true); }).detach();

    const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (timeoutMs);
    auto* messageManager = juce::MessageManager::getInstanceWithoutCreating();
    while (! done->load())
    {
        if (juce::Time::getMillisecondCounter() >= deadline)
            return false;
        if (messageManager != nullptr && messageManager->isThisTheMessageThread())
            messageManager->runDispatchLoopUntil (20);
        else
            juce::Thread::sleep (20);
    }
    return true;
}

} // namespace audium
