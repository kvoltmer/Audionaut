//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

// before any JUCE header: MacTypes' Point/Component names clash with juce's
// once the JUCE declarations are visible (same pattern as Preferences.cpp)
#if __APPLE__
 #include <CoreFoundation/CoreFoundation.h>
#endif

#include "Cli/CliDispatch.h"
#include "Cli/Commands/Commands.h"

#include "Application/UsageAnalytics.h"
#include "Util/Preferences.h"

namespace audium {
namespace cli {

namespace {

#if JUCE_MAC
/**
 * The GUI app is sandboxed on macOS: its preferences - the analytics consent
 * among them - live in its container, invisible to a plain console process
 * through the normal CFPreferences domains. Mirror the consent (and the
 * client id, so the install counts as one) from the container plist into the
 * CLI's own domain; a consent change in the app then governs the CLI from
 * its next invocation.
 */
void mirrorAnalyticsConsentFromAppContainer (Preferences& preferences)
{
    const auto containerDomain =
        juce::File ("~/Library/Containers/com.voltmer-systems.audionaut"
                    "/Data/Library/Preferences/com.voltmer-systems.audionaut").getFullPathName();

    auto copyKey = [&containerDomain] (const char* key) {
        std::string result;
        CFStringRef keyRef = juce::String (key).toCFString();
        CFStringRef appRef = containerDomain.toCFString();
        if (auto value = ::CFPreferencesCopyValue (keyRef, appRef,
                                                   CFSTR ("kCFPreferencesCurrentUser"),
                                                   CFSTR ("kCFPreferencesAnyHost"))) {
            if (::CFGetTypeID (value) == ::CFStringGetTypeID())
                result = juce::String::fromCFString ((CFStringRef) value).toStdString();
            ::CFRelease (value);
        }
        ::CFRelease (appRef);
        ::CFRelease (keyRef);
        return result;
    };

    if (auto consent = copyKey (PreferenceKeys::usageStatsEnabled); ! consent.empty())
        preferences.setValue (PreferenceKeys::usageStatsEnabled, consent);

    if (auto clientId = copyKey (PreferenceKeys::analyticsClientId);
        ! clientId.empty() && ! preferences.valueExists (PreferenceKeys::analyticsClientId))
        preferences.setValue (PreferenceKeys::analyticsClientId, clientId);
}
#endif

/**
 * One `cli_command` analytics event per performed verb - strictly under the
 * same opt-in consent as the GUI (the usageStatsEnabled preference): without
 * consent, or with AUDIONAUT_DISABLE_ANALYTICS set (the test suites set it),
 * nothing is sent and no analytics state is created. The event carries only
 * the verb and its exit code.
 */
void logCliInvocation (const juce::String& verb, int exitCode, CliContext& context)
{
    if (! UsageAnalytics::hasEndpointCredentials()) // no-op builds (tests) skip everything
        return;

    if (juce::SystemStats::getEnvironmentVariable ("AUDIONAUT_DISABLE_ANALYTICS", "").isNotEmpty())
        return;

    std::unique_ptr<Preferences> owned;
    auto* preferences = context.preferences;
    if (preferences == nullptr) {
        owned = std::make_unique<Preferences>();
        owned->init ("Audionaut", "voltmer-systems"); // the app's storage domain
        preferences = owned.get();
#if JUCE_MAC
        mirrorAnalyticsConsentFromAppContainer (*preferences);
#endif
    }

    if (! UsageAnalytics::isEnabled (*preferences))
        return;

    {
        // A tight batch period plus a short flush window: the delivery
        // thread gets one real chance to post (restored events included)
        // before this short-lived process ends. Whatever is still queued is
        // saved and retried on the next consented invocation.
        UsageAnalytics analytics (*preferences, 50);
        juce::StringPairArray parameters;
        parameters.set ("verb", verb);
        parameters.set ("exit_code", juce::String (exitCode));
        analytics.logEvent ("cli_command", parameters);
        juce::Thread::sleep (600);
    }

    // The standalone binary owns no JUCE shutdown that would collect the
    // Analytics singleton; in-app the application's own teardown does.
    if (context.preferences == nullptr)
        juce::Analytics::deleteInstance();
}

} // namespace

const std::vector<CliCommandSpec>& getCliCommands()
{
    static const std::vector<CliCommandSpec> commands = {
        { "info",
          "info <project.audium> [--raw] [--json]",
          "Prints a summary of the project (tracks, clips, tempo).",
          "Prints a summary of the project. --raw dumps the full persistence JSON instead.",
          runInfo },

        { "create",
          "create <project.audium> [--channels N] [--json]",
          "Creates a new empty project with one N-channel track.",
          "Creates a new empty project (default: one stereo track) and saves it as a .audium package.",
          runCreate },

        { "import",
          "import <project.audium> <audio...> [--position SECONDS] [--json]",
          "Imports audio files into the project and saves it.",
          "Imports one or more audio files at the given position (default 0) and saves the project.",
          runImport },

        { "export",
          "export <project.audium> -o <out.wav> [--sample-rate N] [--bit-depth N]\n"
          "                          [--channels N] [--multi-mono] [--start S] [--length S]\n"
          "                          [--region NAME [--track N]] [--json]",
          "Renders the project (or one region) offline to a WAV file.",
          "Renders the project offline (no audio device needed). --multi-mono writes one mono file per "
          "channel. --region bounces a single region instead - always dry, without any clip's gains or fades.",
          runExport },

        { "analyze",
          "analyze <project.audium|audio-file> [--types sbic,beat_degara] [--json]",
          "Runs audio analysis (Essentia) and caches the results.",
          "Analyses the project's audio files (or one given file) and persists the results next to the "
          "project so auto-edit/assemble and the GUI can reuse them. Exits 3 in builds without Essentia.",
          runAnalyze },

        { "auto-edit",
          "auto-edit <project.audium> [--track N] [--clip N] [--measures M]\n"
          "                          [--segments N] [--duration S] [--no-crossfades] [--json]",
          "Segments a clip using cached analysis results.",
          "Applies the auto-edit (segmentation) to a clip. Requires analysis results - run `analyze` first.",
          runAutoEdit },

        { "assemble",
          "assemble <project.audium> [--track N] [--duration S]\n"
          "                          [--mode random|sequential] [--seed N] [--json]",
          "Assembles a new arrangement from the project's regions.",
          "Builds an arrangement of the given duration from the project's regions, sequentially or at random.",
          runAssemble },

        { "split",
          "split <project.audium> --at P [--unit bars|beats|seconds|clocks] [--json]",
          "Splits clips at a timeline position (every track).",
          "Splits the clip under the given position on every track into two, like the GUI's Split command. "
          "Bars/beats are 1-based (bar 1 = timeline start, 4/4); seconds/clocks are absolute. Default unit: bars.",
          runSplit },

        { "create-region",
          "create-region <project.audium> --name NAME --start A --end B\n"
          "                          [--unit bars|beats|seconds|clocks] [--json]",
          "Creates a named region from a timeline range.",
          "Creates a named region from the given range on every track whose clip fully contains it "
          "(the GUI's Create Region command). Bars/beats are 1-based (4/4). Default unit: bars.",
          runCreateRegion },

        { "set-region",
          "set-region <project.audium> --region NAME [--rename NEW] [--track N]\n"
          "                          [--start A] [--end B | --length L] [--unit ...] [--json]",
          "Renames and/or retrims a region.",
          "Renames a region and/or edits its source-relative range (clamped to the source audio). "
          "Retrimming affects every clip that uses the region - clips have no length of their own.",
          runSetRegion },

        { "remove-clip",
          "remove-clip <project.audium> (--at P | --region NAME) [--track N]\n"
          "                          [--unit ...] [--delete-region] [--json]",
          "Removes clip(s) from the timeline.",
          "Removes the clip at a position (one clip; ambiguity across tracks needs --track) or every "
          "placement of a named region. --delete-region also drops the region unless other clips use it.",
          runRemoveClip },

        { "move-clip",
          "move-clip <project.audium> (--at P | --region NAME) --to Q [--track N]\n"
          "                          [--unit ...] [--json]",
          "Moves one clip to a new timeline position.",
          "Moves the addressed clip (the address must match exactly one) to the given position on its track.",
          runMoveClip },

        { "place-clip",
          "place-clip <project.audium> --region NAME --at P [--track N] [--unit ...] [--json]",
          "Places an existing region on the timeline.",
          "Creates a new clip from a named region at the given position, on the track that owns the region "
          "(--track only disambiguates same-named regions).",
          runPlaceClip },

        { "cleanup-regions",
          "cleanup-regions <project.audium> [--json]",
          "Deletes every region no clip uses.",
          "Deletes every region without a clip on the timeline (the GUI's Delete Unused Regions), plus "
          "resource groups left empty. The audio files stay in the package.",
          runCleanupRegions },

        { "clip-gain",
          "clip-gain <project.audium> (--at P | --region NAME) --gain G [--db]\n"
          "                          [--channel C] [--track N] [--unit ...] [--json]",
          "Sets a clip's gain (all channels, or one).",
          "Sets the addressed clip's gain - linear by default, dB with --db - on every destination "
          "channel, or one channel with --channel. The address must match exactly one clip.",
          runClipGain },

        { "clip-fades",
          "clip-fades <project.audium> (--at P | --region NAME) [--fade-in X] [--fade-out X]\n"
          "                          [--fade-in-start X] [--fade-out-end X]\n"
          "                          [--fade-in-curve C] [--fade-out-curve C]\n"
          "                          [--track N] [--unit ...] [--json]",
          "Sets a clip's fade lengths, offsets and curves.",
          "Sets fade lengths and ramp offsets (in the given unit; 0 clears, offsets may be negative to "
          "reach outside the clip) and curve exponents (0.1-4, 0.5 = equal power). Values are clamped "
          "against each other within the clip.",
          runClipFades },

        { "separate",
          "separate <project.audium> [--track N] [--clip N] [--threads N] [--model PATH]\n"
          "                          [--no-mute-source] [--backend demucs|fake] [--json]",
          "Separates a clip into Drums/Bass/Other/Vocals tracks.",
          "Renders the clip, runs the Demucs (htdemucs) stem separator on it and adds one new track "
          "per stem, aligned with the source clip, and mutes the source track (keep it audible with --no-mute-source). Needs the model weights, which the app downloads "
          "into its Models folder (Settings > Separation); --model points at a copy elsewhere. "
          "Takes minutes for a full song; --threads defaults to the physical core count. "
          "--backend fake skips the model and copies the clip into the Vocals stem (for testing).",
          runSeparate },
    };

    return commands;
}

int performCliCommand (const juce::ArgumentList& args, CliContext& context)
{
    if (args.size() == 0)
        return cliCommandNotPerformed;

    auto& first = args.arguments.getReference (0);
    if (first.isOption())
        return cliCommandNotPerformed;

    for (auto& spec : getCliCommands()) {
        if (first.text == spec.verb) {
            auto exitCode = exitFailure;
            try {
                exitCode = spec.run (args, context);
            }
            catch (const std::exception& e) {
                exitCode = context.fail (exitFailure, "exception", e.what());
            }
            catch (...) {
                exitCode = context.fail (exitFailure, "exception", "unknown error");
            }
            logCliInvocation (spec.verb, exitCode, context);
            return exitCode;
        }
    }

    return cliCommandNotPerformed;
}

} // namespace cli
} // namespace audium
