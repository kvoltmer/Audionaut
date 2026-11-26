//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <vector>

#include <JuceHeader.h>

namespace audium
{

/**
 * The Audio Units the system has registered, described without loading
 * one: macOS knows each component's name, manufacturer, kind and version
 * before it is opened. What a scan would say of a plugin, less what only
 * the loaded plugin tells (its channels).
 */
struct AudioUnitRegistry
{
    /// The Audio Units the manager's Audio Unit format would scan, each as the description a scan of
    /// it yields (same identifier, same ids), with no time of a scan (lastInfoUpdateTime). None on
    /// other platforms, or where the manager has no such format.
    static std::vector<juce::PluginDescription> installed (juce::AudioPluginFormatManager& formats);

    /**
     * Starts the system's process that hosts Audio Units out of process
     * (AUHostingService) and keeps it for as long as the returned object
     * lives, by creating one of Apple's own effects in it. Nothing on other
     * platforms.
     *
     * For an application to call before its event loop runs. A hosting
     * process that is first started later - by the first plugin that loads
     * out of process, once the application is up - gets only a part of the
     * mouse movements in its editors: a plugin that tracks the mouse with the
     * cursor pinned (Native Instruments' knobs) then barely follows a drag.
     * Started before the event loop, the process takes every later plugin
     * and their editors track as they should. Found by measurement
     * (2026-11-22, macOS 26.5); the system gives no reason.
     */
    static std::shared_ptr<void> startHostingService();
};

} // namespace audium
