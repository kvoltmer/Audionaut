//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <JuceHeader.h>

namespace audium
{

class PluginHost;

/**
 * A scan of the host's list that leaves the message thread free: the
 * formats' plugin folders, one format after the other, file after file on
 * a thread of its own, each file through the list's scanner (the worker
 * process, where the host has one). A window shows it while it runs.
 *
 * Made and deleted on the message thread, where it also reports: how far
 * it is and the plugin at hand, and at the end - after the last file, or
 * once a cancelled scan gave up the file it was on - what it did. By then
 * the host's list is in line with what is installed (PluginHost::finishScan).
 *
 * Starting it brings back what the user took off the list, as every scan
 * does. Deleting it ends a scan that still runs.
 */
class PluginListScan final : private juce::Timer
{
public:
    struct Outcome
    {
        juce::StringArray failed;    ///< files that could not be loaded as plugins
        juce::StringArray skipped;   ///< files that crashed the worker or took too long, skipped from now on
        int added = 0;               ///< plugins this scan was the first to scan
        bool cancelled = false;
    };

    /// `fraction`: 0 to 1 over all formats. Not the place to delete the scan.
    using Progress = std::function<void (double fraction, const juce::String& pluginName)>;
    /// Called once, unless the scan is deleted first; may delete the scan.
    using Finished = std::function<void (const Outcome&)>;

    /// Starts at once. The host outlives the scan, and the formats are the host's.
    PluginListScan (PluginHost& host, std::vector<juce::AudioPluginFormat*> formats,
                    Progress onProgress, Finished onFinished);
    ~PluginListScan() override;

    /// Ends the scan now: the file at hand is given up without a verdict on it - a plugin that hangs
    /// does not keep the scan until the file's time is up - and the formats still waiting are left out.
    /// The outcome follows as for any scan.
    void cancel();

    /// The host's formats that have something to scan.
    static std::vector<juce::AudioPluginFormat*> scannableFormats (PluginHost& host);

private:
    struct FormatScan;

    void timerCallback() override;
    int numScanned() const;

    PluginHost& host;
    const std::vector<juce::AudioPluginFormat*> formats;
    Progress onProgress;
    Finished onFinished;
    const juce::StringArray skippedBefore;
    int scannedBefore = 0;
    size_t numStarted = 0;                 // formats whose scan began
    Outcome outcome;
    bool finished = false;
    std::unique_ptr<FormatScan> current;   // the format at hand

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginListScan)
};

} // namespace audium
