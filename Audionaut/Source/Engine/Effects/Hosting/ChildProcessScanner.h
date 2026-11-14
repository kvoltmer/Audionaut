//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

namespace audium
{

/**
 * Scans a plugin file in a process of its own: this executable, run as
 * `scan-plugin --json --format <name> <file or identifier>`, which loads the
 * plugin, reports what it found as one JSON envelope on stdout and exits. A
 * plugin that crashes while loading takes that process down, not the app,
 * and the scan goes on; the file is then blacklisted, as JUCE does for a
 * scanner that answers false. The same when the child is not done within
 * the timeout.
 *
 * The app binary doubles as the worker (it runs CLI verbs headlessly), so
 * no helper executable goes into the bundle.
 */
class ChildProcessScanner final : public juce::KnownPluginList::CustomScanner
{
public:
    explicit ChildProcessScanner (juce::File executable, int timeoutMs = 60000);

    bool findPluginTypesFor (juce::AudioPluginFormat& format,
                             juce::OwnedArray<juce::PluginDescription>& result,
                             const juce::String& fileOrIdentifier) override;

    /// The worker's stdout to descriptions; false when it is not the envelope the worker writes (a crash mid-write).
    static bool parseScanEnvelope (const juce::String& output, juce::OwnedArray<juce::PluginDescription>& result);

private:
    juce::File executable;
    int timeoutMs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChildProcessScanner)
};

} // namespace audium
