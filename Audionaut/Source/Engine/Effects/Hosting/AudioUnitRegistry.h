//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

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
};

} // namespace audium
