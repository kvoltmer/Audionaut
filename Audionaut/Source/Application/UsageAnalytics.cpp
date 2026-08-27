//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "UsageAnalytics.h"

using namespace juce;

namespace audium {

// GA4 Measurement Protocol credentials: GA4 Admin -> Data streams -> your
// stream (Measurement ID) -> Measurement Protocol API secrets. With either
// left empty no destination is created and nothing is ever sent. Meant to be
// injected at build time (-DGA4_MEASUREMENT_ID=\"G-...\") so the secret stays
// out of the public sources.
#ifndef GA4_MEASUREMENT_ID
 #define GA4_MEASUREMENT_ID ""
#endif
#ifndef GA4_API_SECRET
 #define GA4_API_SECRET ""
#endif

static const char* const ga4MeasurementId = GA4_MEASUREMENT_ID;
static const char* const ga4ApiSecret    = GA4_API_SECRET;

static constexpr int maxSavedEvents = 64;

/**
    Ships batched analytics events to the GA4 Measurement Protocol endpoint.

    Runs on the analytics thread provided by ThreadedAnalyticsDestination;
    undeliverable events are stored in the preferences on shutdown and
    re-queued on the next launch.
*/
class GA4Destination : public ThreadedAnalyticsDestination
{
public:
    GA4Destination(Preferences& preferences_, const String& clientId_, int initialPeriodMs_) :
        ThreadedAnalyticsDestination("GA4 analytics"),
        preferences(preferences_),
        clientId(clientId_),
        initialPeriodMs(initialPeriodMs_),
        periodMs(initialPeriodMs_)
    {
        startAnalyticsThread(initialPeriodMs);
    }

    ~GA4Destination() override
    {
        // the thread gets this long to deliver or save the queue; the network
        // timeouts below stay inside it so a dead connection can't hang quit
        stopAnalyticsThread(4000);
    }

    int getMaximumBatchSize() override { return 20; } // GA4 caps a request at 25 events

    bool logBatchedEvents(const Array<AnalyticsEvent>& events) override
    {
        if (shouldExit)
            return false;

        auto* root = new DynamicObject();
        root->setProperty("client_id", clientId);

        Array<var> jsonEvents;
        for (auto& event : events) {
            auto* params = new DynamicObject();
            for (auto& key : event.parameters.getAllKeys())
                params->setProperty(key, event.parameters[key]);

            auto* jsonEvent = new DynamicObject();
            jsonEvent->setProperty("name", event.name);
            jsonEvent->setProperty("params", var(params));
            jsonEvents.add(var(jsonEvent));
        }
        root->setProperty("events", jsonEvents);

        const auto url = URL("https://www.google-analytics.com/mp/collect")
                            .withParameter("measurement_id", ga4MeasurementId)
                            .withParameter("api_secret", ga4ApiSecret)
                            .withPOSTData(JSON::toString(var(root), true));

        int statusCode = 0;
        const auto stream = url.createInputStream(
            URL::InputStreamOptions(URL::ParameterHandling::inAddress)
                .withExtraHeaders("Content-Type: application/json")
                .withConnectionTimeoutMs(3000)
                .withStatusCode(&statusCode));

        const auto success = stream != nullptr && statusCode / 100 == 2;

        // back off while the endpoint is unreachable, recover once it is back
        periodMs = success ? initialPeriodMs : jmin(periodMs * 2, 60 * 1000);
        setBatchPeriod(periodMs);

        return success;
    }

    void stopLoggingEvents() override
    {
        shouldExit = true;
    }

private:
    void saveUnloggedEvents(const std::deque<AnalyticsEvent>& eventsToSave) override
    {
        Array<var> jsonEvents;
        for (auto& event : eventsToSave) {
            if (jsonEvents.size() >= maxSavedEvents)
                break;

            auto* params = new DynamicObject();
            for (auto& key : event.parameters.getAllKeys())
                params->setProperty(key, event.parameters[key]);

            auto* jsonEvent = new DynamicObject();
            jsonEvent->setProperty("name", event.name);
            jsonEvent->setProperty("params", var(params));
            jsonEvents.add(var(jsonEvent));
        }

        if (jsonEvents.isEmpty())
            preferences.removeKey(PreferenceKeys::analyticsUnsentEvents);
        else
            preferences.setValue(PreferenceKeys::analyticsUnsentEvents,
                                 JSON::toString(var(jsonEvents), true).toStdString());
        preferences.synchronize();
    }

    void restoreUnloggedEvents(std::deque<AnalyticsEvent>& restoredEventQueue) override
    {
        const auto stored = preferences.getValue(PreferenceKeys::analyticsUnsentEvents);
        if (stored.empty())
            return;

        preferences.removeKey(PreferenceKeys::analyticsUnsentEvents);

        const auto parsed = JSON::parse(String(stored));
        if (auto* jsonEvents = parsed.getArray()) {
            for (auto& jsonEvent : *jsonEvents) {
                AnalyticsEvent event;
                event.name = jsonEvent.getProperty("name", "").toString();
                event.eventType = 0;
                event.timestamp = Time::getMillisecondCounter();

                if (auto* params = jsonEvent.getProperty("params", var()).getDynamicObject())
                    for (auto& property : params->getProperties())
                        event.parameters.set(property.name.toString(),
                                             property.value.toString());

                restoredEventQueue.push_back(event);
            }
        }
    }

    Preferences& preferences;
    const String clientId;
    const int initialPeriodMs;
    int periodMs;
    std::atomic<bool> shouldExit { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GA4Destination)
};


UsageAnalytics::UsageAnalytics(Preferences& preferences_, int batchPeriodMs) :
    preferences(preferences_)
{
    if (! preferences.valueExists(PreferenceKeys::analyticsClientId))
        preferences.setValue(PreferenceKeys::analyticsClientId,
                             Uuid().toString().toStdString());

    // GA4 groups events into sessions by this id; epoch seconds is the
    // conventional value
    sessionId = String(Time::getCurrentTime().toMilliseconds() / 1000);

    Analytics::getInstance()->setSuspended(! isEnabled(preferences));

    if (String(ga4MeasurementId).isNotEmpty() && String(ga4ApiSecret).isNotEmpty())
        Analytics::getInstance()->addDestination(
            new GA4Destination(preferences,
                               preferences.getValue(PreferenceKeys::analyticsClientId),
                               batchPeriodMs));
}

UsageAnalytics::~UsageAnalytics()
{
    // deletes the destinations, which flushes or saves the event queue while
    // the preferences are still alive
    Analytics::getInstance()->getDestinations().clear();
}

bool UsageAnalytics::hasEndpointCredentials()
{
    return String(ga4MeasurementId).isNotEmpty() && String(ga4ApiSecret).isNotEmpty();
}

bool UsageAnalytics::isConsentDecided(Preferences& preferences)
{
    return preferences.valueExists(PreferenceKeys::usageStatsEnabled);
}

bool UsageAnalytics::isEnabled(Preferences& preferences)
{
    return preferences.getValue(PreferenceKeys::usageStatsEnabled) == "true";
}

void UsageAnalytics::setEnabled(bool enabled)
{
    preferences.setValue(PreferenceKeys::usageStatsEnabled, enabled ? "true" : "false");
    preferences.synchronize();

    Analytics::getInstance()->setSuspended(! enabled);
}

void UsageAnalytics::logEvent(const String& name, const StringPairArray& parameters)
{
    auto params = parameters;

    // without these two GA4 omits the event from realtime/engagement reports
    params.set("session_id", sessionId);
    params.set("engagement_time_msec", "100");

    // Analytics drops the event while suspended, i.e. without consent
    Analytics::getInstance()->logEvent(name, params, 0);
}

} // namespace audium
