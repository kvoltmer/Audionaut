//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Cli/HeadlessEngineSession.h"

#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/ClipDynamics.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Provider/TempoProvider.h"

#include <cmath>

namespace audium {
namespace cli {

namespace {

struct ClipMatch {
    std::shared_ptr<AudioTrack> track;
    PlayListItem* item;
};

std::vector<ClipMatch> clipsAtPosition (const AudioTrackContainer& tracks,
                                        double positionClocks,
                                        int trackId)
{
    std::vector<ClipMatch> matches;
    for (auto& track : tracks.getAudioTracks()) {
        if (trackId >= 0 && track->getId() != trackId)
            continue;
        if (auto* item = track->getPlayListContainer()->itemAtAbsolutePosition (positionClocks,
                                                                                audium::clocks))
            matches.push_back ({ track, item });
    }
    return matches;
}

std::vector<ClipMatch> clipsWithRegionName (const AudioTrackContainer& tracks,
                                            const juce::String& regionName,
                                            int trackId)
{
    std::vector<ClipMatch> matches;
    for (auto& track : tracks.getAudioTracks()) {
        if (trackId >= 0 && track->getId() != trackId)
            continue;
        for (auto& item : track->getPlayListContainer()->getPlayListItems())
            if (item->getRegion()->getName() == regionName)
                matches.push_back ({ track, item.get() });
    }
    return matches;
}

nlohmann::json clipJson (const ClipMatch& match, const TempoProvider& tempoProvider)
{
    return { { "track", match.track->getId() },
             { "region", match.item->getRegion()->getName().toStdString() },
             { "positionSeconds",
               tempoProvider.clocksToSeconds (match.item->getAbsolutePosition (audium::clocks)) } };
}

/**
 * Resolves the clip-addressing options shared by remove-clip and move-clip:
 * exactly one of --at or --region, optionally narrowed by --track. The unit
 * is taken by the caller (it may apply to other options too). Returns exitOk
 * with matches filled, or the error exit code after reporting through the
 * context.
 */
int resolveClips (juce::ArgumentList& working,
                  CliContext& context,
                  const AudioTrackContainer& tracks,
                  const TempoProvider& tempoProvider,
                  const juce::String& verb,
                  const juce::String& unit,
                  std::vector<ClipMatch>& matches)
{
    auto atValue = takeOptionValue (working, "--at");
    auto regionName = takeOptionValue (working, "--region");
    auto trackId = takeOptionValue (working, "--track", "-1").getIntValue();

    if (atValue.isEmpty() == regionName.isEmpty())
        return context.fail (exitUsage, "usage",
                             verb.toStdString() + " requires exactly one of --at or --region");

    if (atValue.isNotEmpty()) {
        double positionClocks = 0.0;
        std::string error;
        if (! parseMusicalPosition (atValue, unit, tempoProvider, positionClocks, error))
            return context.fail (exitUsage, "usage", error);
        matches = clipsAtPosition (tracks, positionClocks, trackId);
    }
    else {
        matches = clipsWithRegionName (tracks, regionName, trackId);
    }

    if (matches.empty())
        return context.fail (exitFailure, "no_clip_found", "no clip matches the given address");

    return exitOk;
}

} // namespace

int runRemoveClip (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    bool deleteRegion = working.removeOptionIfFound ("--delete-region");
    bool byPosition = working.containsOption ("--at");
    auto unit = takeOptionValue (working, "--unit", "bars");

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "remove-clip requires an existing <project.audium>");

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    if (! session->getProjectFileStore()->open (projectFile, captureError))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    auto& trackContainer = *session->getAudioTrackContainer();
    auto tempoProvider = trackContainer.getTempoProvider();

    std::vector<ClipMatch> matches;
    if (auto code = resolveClips (working, context, trackContainer, *tempoProvider,
                                  "remove-clip", unit, matches);
        code != exitOk)
        return code;

    // By position, only one clip may be removed per invocation - a position
    // hitting clips on several tracks needs --track. By region name, every
    // placement of that region goes (that is the point of the address).
    if (byPosition && matches.size() > 1)
        return context.fail (exitFailure, "ambiguous_clip",
                             "clips on several tracks span that position; pass --track");

    if (deleteRegion) {
        // The engine does not check for other users of the region; a save
        // with a dangling playlist reference would corrupt the project.
        auto region = matches.front().item->getRegion();
        auto placements = clipsWithRegionName (trackContainer, region->getName(), -1);
        if (placements.size() > matches.size())
            return context.fail (exitFailure, "region_in_use",
                                 "--delete-region refused: other clips still use this region");
    }

    auto removed = nlohmann::json::array();
    for (auto& match : matches) {
        removed.push_back (clipJson (match, *tempoProvider));
        match.track->getPlayListContainer()->deletePlayListItem (match.item, deleteRegion);
    }

    if (! session->getProjectFileStore()->save (projectFile, captureError))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    context.log ("removed " + juce::String (removed.size()) + " clip(s)");
    return context.ok ({ { "removedClips", removed },
                         { "regionDeleted", deleteRegion } });
}

int runMoveClip (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto toValue = takeOptionValue (working, "--to");
    auto unit = takeOptionValue (working, "--unit", "bars");

    if (toValue.isEmpty())
        return context.fail (exitUsage, "usage", "move-clip requires --to <position>");

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "move-clip requires an existing <project.audium>");

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    if (! session->getProjectFileStore()->open (projectFile, captureError))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    auto& trackContainer = *session->getAudioTrackContainer();
    auto tempoProvider = trackContainer.getTempoProvider();

    double toClocks = 0.0;
    if (! parseMusicalPosition (toValue, unit, *tempoProvider, toClocks, error))
        return context.fail (exitUsage, "usage", error);

    std::vector<ClipMatch> matches;
    if (auto code = resolveClips (working, context, trackContainer, *tempoProvider,
                                  "move-clip", unit, matches);
        code != exitOk)
        return code;

    if (matches.size() > 1)
        return context.fail (exitFailure, "ambiguous_clip",
                             "several clips match; pass --track or address by --at");

    auto& match = matches.front();
    auto fromSeconds = tempoProvider->clocksToSeconds (match.item->getAbsolutePosition (audium::clocks));

    match.item->setAbsolutePosition (toClocks, audium::clocks);
    match.track->getPlayListContainer()->sortByPosition();

    if (! session->getProjectFileStore()->save (projectFile, captureError))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    context.log ("moved clip to " + juce::String (toClocks) + " clocks");
    return context.ok ({ { "region", match.item->getRegion()->getName().toStdString() },
                         { "track", match.track->getId() },
                         { "fromSeconds", fromSeconds },
                         { "toSeconds", tempoProvider->clocksToSeconds (toClocks) } });
}

int runPlaceClip (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto regionName = takeOptionValue (working, "--region");
    auto atValue = takeOptionValue (working, "--at");
    auto unit = takeOptionValue (working, "--unit", "bars");
    auto trackId = takeOptionValue (working, "--track", "-1").getIntValue();

    if (regionName.isEmpty() || atValue.isEmpty())
        return context.fail (exitUsage, "usage", "place-clip requires --region and --at");

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "place-clip requires an existing <project.audium>");

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    if (! session->getProjectFileStore()->open (projectFile, captureError))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    auto& trackContainer = *session->getAudioTrackContainer();
    auto tempoProvider = trackContainer.getTempoProvider();

    double positionClocks = 0.0;
    if (! parseMusicalPosition (atValue, unit, *tempoProvider, positionClocks, error))
        return context.fail (exitUsage, "usage", error);

    auto regions = findRegionsByName (trackContainer, regionName, trackId);
    if (regions.empty())
        return context.fail (exitFailure, "region_not_found",
                             "no region named \"" + regionName.toStdString() + "\"");
    if (regions.size() > 1)
        return context.fail (exitFailure, "ambiguous_region",
                             "multiple regions named \"" + regionName.toStdString()
                                 + "\"; pass --track to disambiguate");

    // The clip lands on the track that owns the region - regions live in a
    // track's resource group and cannot play on another track.
    auto track = regions.front().first;
    auto region = regions.front().second;

    auto playList = track->getPlayListContainer();
    auto item = playList->createPlayListItemAtPositionUI (region, positionClocks, audium::clocks);
    if (item == nullptr)
        return context.fail (exitFailure, "place_failed", "could not place the region");
    playList->sortByPosition();

    if (! session->getProjectFileStore()->save (projectFile, captureError))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    context.log ("placed \"" + regionName + "\" at " + juce::String (positionClocks) + " clocks");
    return context.ok ({ { "region", regionName.toStdString() },
                         { "track", track->getId() },
                         { "positionSeconds", tempoProvider->clocksToSeconds (positionClocks) },
                         { "durationSeconds", region->getRegionData (audium::seconds).getLength() } });
}

int runClipGain (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto gainValue = takeOptionValue (working, "--gain");
    bool decibels = working.removeOptionIfFound ("--db");
    auto channelValue = takeOptionValue (working, "--channel");
    auto unit = takeOptionValue (working, "--unit", "bars");

    if (gainValue.isEmpty())
        return context.fail (exitUsage, "usage", "clip-gain requires --gain <value>");

    auto gain = gainValue.getDoubleValue();
    if (decibels)
        gain = std::pow (10.0, gain / 20.0);
    if (gain < 0.0)
        return context.fail (exitUsage, "usage", "gain must not be negative");

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "clip-gain requires an existing <project.audium>");

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    if (! session->getProjectFileStore()->open (projectFile, captureError))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    auto& trackContainer = *session->getAudioTrackContainer();
    auto tempoProvider = trackContainer.getTempoProvider();

    std::vector<ClipMatch> matches;
    if (auto code = resolveClips (working, context, trackContainer, *tempoProvider,
                                  "clip-gain", unit, matches);
        code != exitOk)
        return code;

    if (matches.size() > 1)
        return context.fail (exitFailure, "ambiguous_clip",
                             "several clips match; pass --track or address by --at");

    auto& match = matches.front();
    auto& dynamics = match.item->getDynamics();
    auto numChannels = static_cast<int> (match.track->audioChannelContainer->objects.size());

    if (channelValue.isNotEmpty()) {
        auto channel = channelValue.getIntValue();
        if (channel < 0 || channel >= numChannels)
            return context.fail (exitUsage, "usage",
                                 "--channel must be 0.." + std::to_string (numChannels - 1));
        dynamics.setGain (channel, gain);
    }
    else {
        for (int channel = 0; channel < numChannels; ++channel)
            dynamics.setGain (channel, gain);
    }

    if (! session->getProjectFileStore()->save (projectFile, captureError))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    auto gains = nlohmann::json::array();
    for (int channel = 0; channel < numChannels; ++channel)
        gains.push_back (dynamics.getGain (channel));

    context.log ("clip gain set");
    return context.ok ({ { "region", match.item->getRegion()->getName().toStdString() },
                         { "track", match.track->getId() },
                         { "gains", gains } });
}

int runClipSpeed (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto ratioValue = takeOptionValue (working, "--ratio");
    auto semitonesValue = takeOptionValue (working, "--semitones");
    auto lengthValue = takeOptionValue (working, "--length");
    auto unit = takeOptionValue (working, "--unit", "bars");

    const auto optionsGiven = (ratioValue.isNotEmpty() ? 1 : 0)
                              + (semitonesValue.isNotEmpty() ? 1 : 0)
                              + (lengthValue.isNotEmpty() ? 1 : 0);
    if (optionsGiven != 1)
        return context.fail (exitUsage, "usage",
                             "clip-speed requires exactly one of --ratio, --semitones or --length");

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "clip-speed requires an existing <project.audium>");

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    if (! session->getProjectFileStore()->open (projectFile, captureError))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    auto& trackContainer = *session->getAudioTrackContainer();
    auto tempoProvider = trackContainer.getTempoProvider();

    std::vector<ClipMatch> matches;
    if (auto code = resolveClips (working, context, trackContainer, *tempoProvider,
                                  "clip-speed", unit, matches);
        code != exitOk)
        return code;

    if (matches.size() > 1)
        return context.fail (exitFailure, "ambiguous_clip",
                             "several clips match; pass --track or address by --at");

    auto& match = matches.front();

    auto ratio = 1.0;

    if (ratioValue.isNotEmpty()) {
        ratio = ratioValue.getDoubleValue();
    }
    else if (semitonesValue.isNotEmpty()) {
        ratio = std::pow (2.0, semitonesValue.getDoubleValue() / 12.0);
    }
    else {
        // --length: the timeline duration the clip should occupy
        double lengthClocks = 0.0;
        std::string parseError;
        if (! parseMusicalDuration (lengthValue, unit, *tempoProvider, lengthClocks, parseError))
            return context.fail (exitUsage, "usage", parseError);
        if (lengthClocks <= 0.0)
            return context.fail (exitUsage, "usage", "--length must be positive");

        ratio = match.item->getRegionData (audium::clocks).getLength() / lengthClocks;
    }

    if (ratio < PlayListItem::minSpeedRatio || ratio > PlayListItem::maxSpeedRatio)
        return context.fail (exitUsage, "usage",
                             "the speed ratio must be between "
                             + juce::String (PlayListItem::minSpeedRatio).toStdString() + " and "
                             + juce::String (PlayListItem::maxSpeedRatio).toStdString()
                             + " (got " + juce::String (ratio, 4).toStdString() + ")");

    match.item->setSpeedRatio (ratio);

    if (! session->getProjectFileStore()->save (projectFile, captureError))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    context.log ("clip speed set");
    return context.ok ({ { "region", match.item->getRegion()->getName().toStdString() },
                         { "track", match.track->getId() },
                         { "speedRatio", match.item->getSpeedRatio() },
                         { "durationSeconds", match.item->getDurationTime (audium::seconds) } });
}

int runClipFades (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto fadeIn = takeOptionValue (working, "--fade-in");
    auto fadeOut = takeOptionValue (working, "--fade-out");
    auto fadeInStart = takeOptionValue (working, "--fade-in-start");
    auto fadeOutEnd = takeOptionValue (working, "--fade-out-end");
    auto fadeInCurve = takeOptionValue (working, "--fade-in-curve");
    auto fadeOutCurve = takeOptionValue (working, "--fade-out-curve");
    auto unit = takeOptionValue (working, "--unit", "bars");

    if (fadeIn.isEmpty() && fadeOut.isEmpty() && fadeInStart.isEmpty() && fadeOutEnd.isEmpty()
        && fadeInCurve.isEmpty() && fadeOutCurve.isEmpty())
        return context.fail (exitUsage, "usage",
                             "clip-fades needs at least one of --fade-in, --fade-out, "
                             "--fade-in-start, --fade-out-end, --fade-in-curve, --fade-out-curve");

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "clip-fades requires an existing <project.audium>");

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    if (! session->getProjectFileStore()->open (projectFile, captureError))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    auto& trackContainer = *session->getAudioTrackContainer();
    auto tempoProvider = trackContainer.getTempoProvider();

    std::vector<ClipMatch> matches;
    if (auto code = resolveClips (working, context, trackContainer, *tempoProvider,
                                  "clip-fades", unit, matches);
        code != exitOk)
        return code;

    if (matches.size() > 1)
        return context.fail (exitFailure, "ambiguous_clip",
                             "several clips match; pass --track or address by --at");

    auto& match = matches.front();
    auto& dynamics = match.item->getDynamics();

    auto regionLengthClocks = match.item->getRegionData (audium::clocks).getLength();
    if (regionLengthClocks <= 0.0)
        return context.fail (exitFailure, "empty_clip", "the clip has no length");

    // Durations here may be zero (clear a fade) or negative (ramp offsets
    // reaching outside the clip), so this converts without the sign checks of
    // parseMusicalDuration. The engine stores fades as fractions of the
    // region length and clamps them against each other.
    auto toFraction = [&] (const juce::String& value, double& outFraction) {
        double clocks = 0.0;
        const auto numeric = value.getDoubleValue();
        if (unit == "bars")
            clocks = numeric * clocksPerBar;
        else if (unit == "beats")
            clocks = numeric * clocksPerBeat;
        else if (unit == "seconds")
            clocks = tempoProvider->secondsToClocks (numeric);
        else if (unit == "clocks")
            clocks = numeric;
        else {
            error = "--unit must be bars, beats, seconds or clocks";
            return false;
        }
        outFraction = clocks / regionLengthClocks;
        return true;
    };

    bool valuesPushed = false;
    double fraction = 0.0;

    if (fadeIn.isNotEmpty()) {
        if (! toFraction (fadeIn, fraction))
            return context.fail (exitUsage, "usage", error);
        if (fraction < 0.0)
            return context.fail (exitUsage, "usage", "--fade-in must not be negative");
        valuesPushed |= dynamics.setFadeIn (fraction);
    }
    if (fadeOut.isNotEmpty()) {
        if (! toFraction (fadeOut, fraction))
            return context.fail (exitUsage, "usage", error);
        if (fraction < 0.0)
            return context.fail (exitUsage, "usage", "--fade-out must not be negative");
        valuesPushed |= dynamics.setFadeOut (fraction);
    }
    if (fadeInStart.isNotEmpty()) {
        if (! toFraction (fadeInStart, fraction))
            return context.fail (exitUsage, "usage", error);
        valuesPushed |= dynamics.setFadeInStart (fraction);
    }
    if (fadeOutEnd.isNotEmpty()) {
        if (! toFraction (fadeOutEnd, fraction))
            return context.fail (exitUsage, "usage", error);
        valuesPushed |= dynamics.setFadeOutEnd (fraction);
    }
    if (fadeInCurve.isNotEmpty())
        dynamics.setFadeInCurve (fadeInCurve.getDoubleValue());
    if (fadeOutCurve.isNotEmpty())
        dynamics.setFadeOutCurve (fadeOutCurve.getDoubleValue());

    if (! session->getProjectFileStore()->save (projectFile, captureError))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    context.log ("clip fades set");
    return context.ok ({ { "region", match.item->getRegion()->getName().toStdString() },
                         { "track", match.track->getId() },
                         { "fadeInSeconds", dynamics.getFadeIn (audium::seconds) },
                         { "fadeOutSeconds", dynamics.getFadeOut (audium::seconds) },
                         { "fadeInStartSeconds", dynamics.getFadeInStart (audium::seconds) },
                         { "fadeOutEndSeconds", dynamics.getFadeOutEnd (audium::seconds) },
                         { "fadeInCurve", dynamics.getFadeInCurve() },
                         { "fadeOutCurve", dynamics.getFadeOutCurve() },
                         { "otherValuesAdjusted", valuesPushed } });
}

} // namespace cli
} // namespace audium
