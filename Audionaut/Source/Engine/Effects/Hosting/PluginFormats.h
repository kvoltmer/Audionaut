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
    /// The formats this platform hosts, added to the manager.
    static void addPlatformFormats (juce::AudioPluginFormatManager& manager);

    /// Whether this process runs in the macOS app sandbox (the app, and the scan helper it starts).
    static bool isSandboxed();

    /// Whether the system asks the user before it loads this plugin here: an Audio Unit (version 2)
    /// that does not say of itself that it keeps to the sandbox's rules, in a sandboxed process. The
    /// system opens such a plugin only in a process that carries the entitlement to host them
    /// (com.apple.security.temporary-exception.audio-unit-host: the app, not the scan helper), and the
    /// first time it asks the user to lower that app's security settings. Told from the identifier,
    /// without loading anything.
    static bool asksUserBeforeLoading (const juce::String& formatName, const juce::String& fileOrIdentifier);

    /**
     * Message thread: runs `fn` on a thread of its own while this thread
     * pumps messages, for the plugin calls that refuse to run on the message
     * thread and want it free (an AUv3 scan). False when `fn` is not done
     * within the timeout; it then goes on in the background.
     */
    static bool runWhilePumping (std::function<void()> fn, int timeoutMs);
};

} // namespace audium
