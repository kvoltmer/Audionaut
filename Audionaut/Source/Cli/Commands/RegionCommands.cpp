//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Cli/HeadlessEngineSession.h"

#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioRegionAdapter.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Provider/TempoProvider.h"

#include <set>

namespace audium {
namespace cli {

namespace {

std::set<juce::String> regionNames (const AudioRegionAdapter& adapter)
{
    std::set<juce::String> names;
    for (auto& region : adapter.getAudioRegions())
        names.insert (region->getName());
    return names;
}

nlohmann::json newRegionNames (const AudioRegionAdapter& adapter,
                               const std::set<juce::String>& baseline)
{
    auto created = nlohmann::json::array();
    for (auto& region : adapter.getAudioRegions())
        if (baseline.count (region->getName()) == 0)
            created.push_back (region->getName().toStdString());
    return created;
}

} // namespace

int runSplit (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto atValue = takeOptionValue (working, "--at");
    auto unit = takeOptionValue (working, "--unit", "bars");

    if (atValue.isEmpty())
        return context.fail (exitUsage, "usage", "split requires --at <position>");

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "split requires an existing <project.audium>");

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    if (! session->getProjectFileStore()->open (projectFile, captureError))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    auto tempoProvider = session->getAudioTrackContainer()->getTempoProvider();

    double positionClocks = 0.0;
    if (! parseMusicalPosition (atValue, unit, *tempoProvider, positionClocks, error))
        return context.fail (exitUsage, "usage", error);

    auto& adapter = session->getAudioTrackContainer()->getAudioRegionAdapter();

    if (! adapter.canSplitAnyRegionAtPosition (positionClocks, audium::clocks))
        return context.fail (exitFailure, "nothing_to_split",
                             "no clip spans the given position on any track");

    auto baseline = regionNames (adapter);
    adapter.splitRegions (positionClocks, audium::clocks);
    auto created = newRegionNames (adapter, baseline);

    if (! session->getProjectFileStore()->save (projectFile, captureError))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    context.log ("split applied at " + juce::String (positionClocks) + " clocks");
    return context.ok ({ { "positionClocks", positionClocks },
                         { "positionSeconds", tempoProvider->clocksToSeconds (positionClocks) },
                         { "createdRegions", created } });
}

int runCreateRegion (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto name = takeOptionValue (working, "--name").trim();
    auto startValue = takeOptionValue (working, "--start");
    auto endValue = takeOptionValue (working, "--end");
    auto unit = takeOptionValue (working, "--unit", "bars");

    if (name.isEmpty())
        return context.fail (exitUsage, "usage", "create-region requires --name <name>");
    if (startValue.isEmpty() || endValue.isEmpty())
        return context.fail (exitUsage, "usage", "create-region requires --start and --end");

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "create-region requires an existing <project.audium>");

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    if (! session->getProjectFileStore()->open (projectFile, captureError))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    auto tempoProvider = session->getAudioTrackContainer()->getTempoProvider();

    double startClocks = 0.0, endClocks = 0.0;
    if (! parseMusicalPosition (startValue, unit, *tempoProvider, startClocks, error)
        || ! parseMusicalPosition (endValue, unit, *tempoProvider, endClocks, error))
        return context.fail (exitUsage, "usage", error);

    if (endClocks <= startClocks)
        return context.fail (exitUsage, "usage", "--end must be after --start");

    auto& adapter = session->getAudioTrackContainer()->getAudioRegionAdapter();
    adapter.setSelectedRange ({ startClocks, endClocks }, audium::clocks);

    if (! adapter.canCreateRegion())
        return context.fail (exitFailure, "no_clip_in_range",
                             "no clip fully contains the given range on any track");

    auto baseline = regionNames (adapter);
    adapter.createRegionsFromSelection (name, true);
    auto created = newRegionNames (adapter, baseline);

    if (created.empty())
        return context.fail (exitFailure, "create_failed", "no region was created");

    if (! session->getProjectFileStore()->save (projectFile, captureError))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    context.log ("created region(s) from " + juce::String (startClocks)
                 + " to " + juce::String (endClocks) + " clocks");
    return context.ok ({ { "name", name.toStdString() },
                         { "startClocks", startClocks },
                         { "endClocks", endClocks },
                         { "startSeconds", tempoProvider->clocksToSeconds (startClocks) },
                         { "endSeconds", tempoProvider->clocksToSeconds (endClocks) },
                         { "createdRegions", created } });
}

int runSetRegion (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto regionName = takeOptionValue (working, "--region");
    auto newName = takeOptionValue (working, "--rename").trim();
    auto startValue = takeOptionValue (working, "--start");
    auto endValue = takeOptionValue (working, "--end");
    auto lengthValue = takeOptionValue (working, "--length");
    auto unit = takeOptionValue (working, "--unit", "bars");
    auto trackId = takeOptionValue (working, "--track", "-1").getIntValue();

    if (regionName.isEmpty())
        return context.fail (exitUsage, "usage", "set-region requires --region <name>");
    if (newName.isEmpty() && startValue.isEmpty() && endValue.isEmpty() && lengthValue.isEmpty())
        return context.fail (exitUsage, "usage",
                             "set-region needs at least one of --rename, --start, --end, --length");
    if (endValue.isNotEmpty() && lengthValue.isNotEmpty())
        return context.fail (exitUsage, "usage", "--end and --length are mutually exclusive");

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "set-region requires an existing <project.audium>");

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    if (! session->getProjectFileStore()->open (projectFile, captureError))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    auto& trackContainer = *session->getAudioTrackContainer();
    auto tempoProvider = trackContainer.getTempoProvider();

    auto matches = findRegionsByName (trackContainer, regionName, trackId);
    if (matches.empty())
        return context.fail (exitFailure, "region_not_found",
                             "no region named \"" + regionName.toStdString() + "\"");
    if (matches.size() > 1)
        return context.fail (exitFailure, "ambiguous_region",
                             "multiple regions named \"" + regionName.toStdString()
                                 + "\"; pass --track to disambiguate");

    auto region = matches.front().second;

    // Retrim first, so a rename that fails later cannot leave a half-applied
    // range on disk (nothing is saved before both succeed anyway).
    if (startValue.isNotEmpty() || endValue.isNotEmpty() || lengthValue.isNotEmpty()) {
        // The range is source-relative: a position within the resource
        // group's audio, in the same units as timeline positions.
        auto range = region->getRegionData (audium::clocks);
        double newStart = range.getStart(), newEnd = range.getEnd(), parsed = 0.0;

        if (startValue.isNotEmpty()) {
            if (! parseMusicalPosition (startValue, unit, *tempoProvider, parsed, error))
                return context.fail (exitUsage, "usage", error);
            newStart = parsed;
        }
        if (endValue.isNotEmpty()) {
            if (! parseMusicalPosition (endValue, unit, *tempoProvider, parsed, error))
                return context.fail (exitUsage, "usage", error);
            newEnd = parsed;
        }
        else if (lengthValue.isNotEmpty()) {
            if (! parseMusicalDuration (lengthValue, unit, *tempoProvider, parsed, error))
                return context.fail (exitUsage, "usage", error);
            newEnd = newStart + parsed;
        }

        if (newEnd <= newStart)
            return context.fail (exitUsage, "usage", "the resulting range is empty");

        AudioRegionData::tRange newRange (newStart, newEnd);
        region->validateData (newRange, audium::clocks); // clamps to the source audio
        region->setRegionData (newRange, audium::clocks);
    }

    if (newName.isNotEmpty() && newName != regionName) {
        if (! findRegionsByName (trackContainer, newName).empty())
            return context.fail (exitFailure, "name_taken",
                                 "a region named \"" + newName.toStdString() + "\" already exists");
        region->setName (newName);
    }

    // Clips have no length of their own - every placement of this region is
    // affected by a retrim.
    int affectedClips = 0;
    for (auto& track : trackContainer.getAudioTracks())
        for (auto& item : track->getPlayListContainer()->getPlayListItems())
            if (item->getRegion() == region)
                ++affectedClips;

    if (! session->getProjectFileStore()->save (projectFile, captureError))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    auto applied = region->getRegionData (audium::seconds);
    context.log ("set-region applied to \"" + region->getName() + "\"");
    return context.ok ({ { "region", region->getName().toStdString() },
                         { "startSeconds", applied.getStart() },
                         { "endSeconds", applied.getEnd() },
                         { "affectedClips", affectedClips } });
}

int runCleanupRegions (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "cleanup-regions requires an existing <project.audium>");

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    if (! session->getProjectFileStore()->open (projectFile, captureError))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    auto& adapter = session->getAudioTrackContainer()->getAudioRegionAdapter();

    auto baseline = regionNames (adapter);
    session->getAudioTrackContainer()->deleteUnusedRegions();
    auto remaining = regionNames (adapter);

    auto removed = nlohmann::json::array();
    for (auto& name : baseline)
        if (remaining.count (name) == 0)
            removed.push_back (name.toStdString());

    if (! session->getProjectFileStore()->save (projectFile, captureError))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    context.log ("removed " + juce::String (removed.size()) + " unused region(s)");
    return context.ok ({ { "removedRegions", removed },
                         { "remainingRegions", remaining.size() } });
}

} // namespace cli
} // namespace audium
