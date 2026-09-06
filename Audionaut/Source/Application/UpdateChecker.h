//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Util/Preferences.h"

namespace audium {

/**
 * @class UpdateChecker
 * @brief Channel-aware "is a newer Audionaut available?" check.
 *
 * The version source follows the distribution channel: the macOS build
 * ships through the Mac App Store, so it asks Apple's lookup API and the
 * update dialog opens the store page; the Windows/Linux builds ship as
 * GitHub Releases, so they ask the latest-release API and open the release
 * page. Either way it is one small anonymous HTTPS GET - no telemetry.
 *
 * Two entry points: checkOnStartupIfDue() is the automatic path (honours
 * the Settings toggle, throttled to one attempt per day, silent unless a
 * new version appears, and nags at most once per remote version);
 * checkNow(true) is the menu command and always answers - up to date, the
 * update dialog, or the error.
 *
 * The fetch runs on a detached thread and hops back to the message thread
 * through a WeakReference, so a check in flight at shutdown fizzles out
 * harmlessly.
 */
class UpdateChecker
{
public:
    explicit UpdateChecker (Preferences& preferences);
    ~UpdateChecker() = default;

    /// The menu command (userRequested = true) or the startup path's
    /// actual fetch (false). Re-entrant calls while a check is in flight
    /// are dropped.
    void checkNow (bool userRequested);

    /// The automatic path: only if the Settings toggle allows it and the
    /// last attempt is more than a day old.
    void checkOnStartupIfDue();

    /// The Settings toggle's value; an absent key means enabled.
    static bool readEnabled (Preferences& preferences)
    {
        if (! preferences.valueExists (PreferenceKeys::updateCheckEnabled))
            return true;

        return preferences.getValue (PreferenceKeys::updateCheckEnabled) == "true";
    }

private:
    struct Result
    {
        bool fetched = false;         // transport + parse succeeded
        juce::String latestVersion;   // e.g. "1.4.0" (tag's 'v' stripped by the compare)
        juce::String pageUrl;         // store page / release page
        juce::String error;           // set when fetched is false
    };

    /// Blocking GET + parse; runs on the worker thread.
    static Result fetchLatest();

    /// Message-thread presentation of a finished fetch.
    void presentResult (const Result& result, bool userRequested);

    Preferences& preferences;
    std::atomic<bool> checkInFlight { false };

    JUCE_DECLARE_WEAK_REFERENCEABLE (UpdateChecker)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UpdateChecker)
};

} // namespace audium
