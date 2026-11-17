//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include <nlohmann/json.hpp>

#include "ChildProcessScanner.h"

namespace audium
{

/**
 * The scan worker's side of ChildProcessScanner: the process that loads the
 * plugin files in the app's place. It is what the `scan-plugin` verb runs,
 * and all there is to the scan helper executable (Audionaut/ScanHelper),
 * which is why it needs nothing of the engine.
 *
 * The process is a scan worker and ends with either call: a thread still
 * inside a plugin when the time was up goes on using the formats and wants
 * the message loop, so a worker that got stuck is left as it is. Whoever
 * started the process takes the failure as the verdict.
 *
 * `ownsMessageManager`: false where the process already runs a message loop
 * (the app's in-app CLI); otherwise the worker makes one for its plugins.
 */
struct ScanWorker
{
    /// Many files, one after the other, for a scanner at the other end of the pipe. The process's exit code.
    static int serve (const juce::String& pipeName, bool ownsMessageManager = true);

    /// One file: `{"ok": true, "plugins": [xml, ...]}`, or `{"ok": false, "code", "message"}`.
    static nlohmann::json scanOneFile (const ChildProcessScanner::Request& request, bool ownsMessageManager = true);
};

} // namespace audium
