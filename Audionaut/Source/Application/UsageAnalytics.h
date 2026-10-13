//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Util/Preferences.h"

namespace audium {

/**
    Anonymous usage statistics, sent to Google Analytics 4 via the
    Measurement Protocol.

    Collection is strictly opt-in: until the user has agreed (the
    usageStatsEnabled preference is "true") every event is dropped. The
    per-install client id is a random UUID - no personal data, no hardware
    identifiers. Events that cannot be delivered (offline, quit) are saved
    to the preferences and retried on the next launch.

    The GA4 endpoint credentials live in UsageAnalytics.cpp; with them left
    empty this class is a no-op apart from the consent bookkeeping.
*/
class UsageAnalytics
{
public:
    /** batchPeriodMs is how soon the delivery thread first posts queued
        events; the GUI keeps the relaxed default, short-lived CLI runs pass
        a small value so a brief flush window suffices. */
    explicit UsageAnalytics(Preferences& preferences, int batchPeriodMs = 1000);
    ~UsageAnalytics();

    /** True when GA4 credentials were compiled in - without them the class
        is a no-op and callers can skip their flush windows entirely. */
    static bool hasEndpointCredentials();

    /** True once the user has answered the first-run consent question. */
    static bool isConsentDecided(Preferences& preferences);

    /** The stored consent; off unless explicitly granted. */
    static bool isEnabled(Preferences& preferences);

    /** Stores the consent and suspends/resumes event submission. */
    void setEnabled(bool enabled);

    /** Queues an event if the user has opted in; drops it silently otherwise. */
    void logEvent(const juce::String& name, const juce::StringPairArray& parameters = {});

private:
    Preferences& preferences;
    juce::String sessionId;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UsageAnalytics)
};

} // namespace audium
