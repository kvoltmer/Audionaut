//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Cli/HeadlessEngineSession.h"

#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/PlayList/PlayListContainer.h"

namespace audium {
namespace cli {

namespace {

std::shared_ptr<AudioTrack> trackById (const AudioTrackContainer& tracks, int trackId)
{
    for (auto& track : tracks.getAudioTracks())
        if (track->getId() == trackId)
            return track;
    return nullptr;
}

} // namespace

int runRemoveTrack (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto trackValue = takeOptionValue (working, "--track");

    if (trackValue.isEmpty())
        return context.fail (exitUsage, "usage", "remove-track requires --track <id>");

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "remove-track requires an existing <project.audium>");

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    if (! session->getProjectFileStore()->open (projectFile, captureError))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    auto& trackContainer = *session->getAudioTrackContainer();

    auto track = trackById (trackContainer, trackValue.getIntValue());
    if (track == nullptr)
        return context.fail (exitFailure, "track_not_found",
                             "no track with id " + trackValue.toStdString());

    auto removedId = track->getId();
    auto removedName = track->getAudioTrackName().toStdString();
    auto removedClips = static_cast<int> (track->getPlayListContainer()->getPlayListItems().size());

    if (! trackContainer.deleteAudioTrack (track))
        return context.fail (exitFailure, "remove_failed", "could not remove the track");

    if (! session->getProjectFileStore()->save (projectFile, captureError))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    context.log ("removed track " + juce::String (removedId) + " (\"" + removedName + "\")");
    return context.ok ({ { "removedTrack", removedId },
                         { "name", removedName },
                         { "removedClips", removedClips },
                         { "remainingTracks",
                           static_cast<int> (trackContainer.getAudioTracks().size()) } });
}

int runRemoveChannel (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto trackValue = takeOptionValue (working, "--track");
    auto channelValue = takeOptionValue (working, "--channel");

    if (trackValue.isEmpty() || channelValue.isEmpty())
        return context.fail (exitUsage, "usage", "remove-channel requires --track <id> and --channel <index>");

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "remove-channel requires an existing <project.audium>");

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    if (! session->getProjectFileStore()->open (projectFile, captureError))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    auto& trackContainer = *session->getAudioTrackContainer();

    auto track = trackById (trackContainer, trackValue.getIntValue());
    if (track == nullptr)
        return context.fail (exitFailure, "track_not_found",
                             "no track with id " + trackValue.toStdString());

    auto numChannels = static_cast<int> (track->audioChannelContainer->objects.size());
    auto channelIndex = channelValue.getIntValue();
    if (channelIndex < 0 || channelIndex >= numChannels)
        return context.fail (exitUsage, "usage",
                             "--channel must be 0.." + std::to_string (numChannels - 1));

    auto channel = track->audioChannelContainer->objects[(size_t) channelIndex];
    if (! track->deleteChannel (channel.get()))
        return context.fail (exitFailure, "remove_failed", "could not remove the channel");

    if (! session->getProjectFileStore()->save (projectFile, captureError))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    context.log ("removed channel " + juce::String (channelIndex) + " from track "
                 + juce::String (track->getId()));
    return context.ok ({ { "track", track->getId() },
                         { "removedChannel", channelIndex },
                         { "remainingChannels",
                           static_cast<int> (track->audioChannelContainer->objects.size()) } });
}

} // namespace cli
} // namespace audium
