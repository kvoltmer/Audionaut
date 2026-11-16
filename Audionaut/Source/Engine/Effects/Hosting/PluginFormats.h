//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>

#include <JuceHeader.h>

namespace audium
{

/**
 * The plugin formats a process of this platform hosts, and how their plugins
 * are loaded. Apart from JUCE this needs nothing of the engine: the scan
 * helper executable is built from it too (Audionaut/ScanHelper).
 */
struct PluginFormats
{
    /// The formats this platform hosts, added to the manager. Plugins that need the user's consent to
    /// lowered security settings are not loaded until applyLoweredSecurity() says so.
    static void addPlatformFormats (juce::AudioPluginFormatManager& manager);

    /// Whether the manager's formats load the plugins the system asks the user about (an AUv2 without
    /// the sandbox-safe flag, in a sandboxed process: out of process, after the user's consent).
    static void applyLoweredSecurity (juce::AudioPluginFormatManager& manager, bool allowed);

    /**
     * Message thread: runs `fn` on a thread of its own while this thread
     * pumps messages, for the plugin calls that refuse to run on the message
     * thread and want it free (an AUv3 scan). False when `fn` is not done
     * within the timeout; it then goes on in the background.
     */
    static bool runWhilePumping (std::function<void()> fn, int timeoutMs);
};

} // namespace audium
