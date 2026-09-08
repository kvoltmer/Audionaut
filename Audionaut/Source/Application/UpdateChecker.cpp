//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "UpdateChecker.h"
#include "Util/VersionCompare.h"

namespace audium {

namespace {

// One attempt per day on the automatic path.
constexpr juce::int64 throttleSeconds = 24 * 60 * 60;

#if JUCE_MAC
// The macOS build ships through the Mac App Store; the lookup reports the
// version actually approved there (which can lag a GitHub tag).
const char* const endpointUrl =
    "https://itunes.apple.com/lookup?bundleId=com.voltmer-systems.audionaut";
const char* const fallbackPageUrl = "https://apps.apple.com/app/id6743627933";
#else
const char* const endpointUrl =
    "https://api.github.com/repos/kvoltmer/Audionaut/releases/latest";
const char* const fallbackPageUrl = "https://github.com/kvoltmer/Audionaut/releases/latest";
#endif

juce::String openButtonText()
{
#if JUCE_MAC
    return TRANS ("View in App Store");
#else
    return TRANS ("Download");
#endif
}

} // namespace

UpdateChecker::UpdateChecker (Preferences& preferences_) :
    preferences (preferences_)
{
}

void UpdateChecker::checkOnStartupIfDue()
{
    if (! readEnabled (preferences))
        return;

    const auto now = juce::Time::getCurrentTime().toMilliseconds() / 1000;
    const auto last = juce::String (preferences.getValue (PreferenceKeys::lastUpdateCheckTime, "0"))
                          .getLargeIntValue();

    if (now - last < throttleSeconds)
        return;

    checkNow (false);
}

void UpdateChecker::checkNow (bool userRequested)
{
    if (checkInFlight.exchange (true))
        return;

    preferences.setValue (PreferenceKeys::lastUpdateCheckTime,
                          juce::String (juce::Time::getCurrentTime().toMilliseconds() / 1000)
                              .toStdString());
    preferences.synchronize();

    juce::Thread::launch ([safeThis = juce::WeakReference<UpdateChecker> (this), userRequested] {
        const auto result = fetchLatest();

        juce::MessageManager::callAsync ([safeThis, result, userRequested] {
            if (safeThis == nullptr)
                return;

            safeThis->checkInFlight.store (false);
            safeThis->presentResult (result, userRequested);
        });
    });
}

UpdateChecker::Result UpdateChecker::fetchLatest()
{
    Result result;

    // GitHub rejects requests without a User-Agent; Apple doesn't mind one.
    juce::String headers ("User-Agent: Audionaut/" + juce::String (ProjectInfo::versionString));
#if ! JUCE_MAC
    headers << "\r\nAccept: application/vnd.github+json";
#endif

    int statusCode = 0;
    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs (5000)
                             .withNumRedirectsToFollow (5)
                             .withExtraHeaders (headers)
                             .withStatusCode (&statusCode);

    const auto stream = juce::URL (endpointUrl).createInputStream (options);

    if (stream == nullptr)
    {
        result.error = TRANS ("The update server could not be reached.");
        return result;
    }

    const auto body = stream->readEntireStreamAsString();

    if (statusCode != 200)
    {
        result.error = TRANS ("The update server answered with status ") + juce::String (statusCode) + ".";
        return result;
    }

    const auto parsed = juce::JSON::parse (body);

#if JUCE_MAC
    const auto entry = parsed.getProperty ("results", {})[0];
    result.latestVersion = entry.getProperty ("version", {}).toString();
    result.pageUrl = entry.getProperty ("trackViewUrl", {}).toString();
#else
    result.latestVersion = parsed.getProperty ("tag_name", {}).toString();
    result.pageUrl = parsed.getProperty ("html_url", {}).toString();
#endif

    if (result.latestVersion.isEmpty())
    {
        result.error = TRANS ("The update server's answer made no sense.");
        return result;
    }

    if (result.pageUrl.isEmpty())
        result.pageUrl = fallbackPageUrl;

    result.fetched = true;
    return result;
}

void UpdateChecker::presentResult (const Result& result, bool userRequested)
{
    const juce::String current (ProjectInfo::versionString);

    if (! result.fetched)
    {
        // the automatic path never bothers anyone about a network hiccup
        if (userRequested)
            juce::NativeMessageBox::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                TRANS ("Check for Updates"), result.error);
        return;
    }

    if (! isNewerVersion (result.latestVersion, current))
    {
        if (userRequested)
            juce::NativeMessageBox::showMessageBoxAsync (
                juce::MessageBoxIconType::InfoIcon,
                TRANS ("Check for Updates"),
                TRANS ("You're up to date - Audionaut ") + current
                    + TRANS (" is the newest version."));
        return;
    }

    const auto displayVersion = result.latestVersion.startsWithIgnoreCase ("v")
                                    ? result.latestVersion.substring (1)
                                    : result.latestVersion;

    // the automatic path announces each new version once; the menu command
    // always answers
    if (! userRequested
        && preferences.getValue (PreferenceKeys::lastNotifiedVersion) == displayVersion.toStdString())
        return;

    preferences.setValue (PreferenceKeys::lastNotifiedVersion, displayVersion.toStdString());
    preferences.synchronize();

    auto options = juce::MessageBoxOptions::makeOptionsOkCancel (
        juce::MessageBoxIconType::InfoIcon,
        TRANS ("Update available"),
        TRANS ("Audionaut ") + displayVersion + TRANS (" is available - you have ") + current + ".",
        openButtonText(),
        TRANS ("Later"));

    const auto pageUrl = result.pageUrl;
    juce::NativeMessageBox::showAsync (options, [pageUrl] (int pressed) {
        if (pressed == 0)
            juce::URL (pageUrl).launchInDefaultBrowser();
    });
}

} // namespace audium
