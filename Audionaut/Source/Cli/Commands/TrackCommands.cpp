//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Cli/CommandSession.h"

#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/PlayList/PlayListContainer.h"

namespace audium {
namespace cli {

std::shared_ptr<AudioTrack> findTrackById (const AudioTrackContainer& tracks, int trackId)
{
    for (auto& track : tracks.getAudioTracks())
        if (track->getId() == trackId)
            return track;
    return nullptr;
}

int runRemoveTrack (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto trackValue = takeOptionValue (working, "--track");

    if (trackValue.isEmpty())
        return context.fail (exitUsage, "usage", "remove-track requires --track <id>");

    return runProjectEdit (working, context, "remove-track", [&] (ProjectSession& session, EditOutcome& outcome)
    {
        auto& trackContainer = *session->getAudioTrackContainer();

        auto track = findTrackById (trackContainer, trackValue.getIntValue());
        if (track == nullptr)
            return context.fail (exitFailure, "track_not_found",
                                 "no track with id " + trackValue.toStdString());

        auto removedId = track->getId();
        auto removedName = track->getAudioTrackName().toStdString();
        auto removedClips = static_cast<int> (track->getPlayListContainer()->getPlayListItems().size());

        if (! trackContainer.deleteAudioTrack (track))
            return context.fail (exitFailure, "remove_failed", "could not remove the track");

        outcome.log = "removed track " + juce::String (removedId) + " (\"" + removedName + "\")";
        outcome.result = { { "removedTrack", removedId },
                           { "name", removedName },
                           { "removedClips", removedClips },
                           { "remainingTracks",
                             static_cast<int> (trackContainer.getAudioTracks().size()) } };
        return exitOk;
    });
}

int runRemoveChannel (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto trackValue = takeOptionValue (working, "--track");
    auto channelValue = takeOptionValue (working, "--channel");

    if (trackValue.isEmpty() || channelValue.isEmpty())
        return context.fail (exitUsage, "usage", "remove-channel requires --track <id> and --channel <index>");

    return runProjectEdit (working, context, "remove-channel", [&] (ProjectSession& session, EditOutcome& outcome)
    {
        auto& trackContainer = *session->getAudioTrackContainer();

        auto track = findTrackById (trackContainer, trackValue.getIntValue());
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

        outcome.log = "removed channel " + juce::String (channelIndex) + " from track "
                      + juce::String (track->getId());
        outcome.result = { { "track", track->getId() },
                           { "removedChannel", channelIndex },
                           { "remainingChannels",
                             static_cast<int> (track->audioChannelContainer->objects.size()) } };
        return exitOk;
    });
}

int runTrackPan (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto trackValue = takeOptionValue (working, "--track");
    auto channelValue = takeOptionValue (working, "--channel");
    auto panValue = takeOptionValue (working, "--pan");

    if (trackValue.isEmpty() || panValue.isEmpty())
        return context.fail (exitUsage, "usage", "track-pan requires --track <id> and --pan <-1..1>");

    auto pan = panValue.getDoubleValue();
    if (pan < -1.0 || pan > 1.0 || ! panValue.containsAnyOf ("0123456789"))
        return context.fail (exitUsage, "usage", "--pan must be between -1 (left) and 1 (right)");

    return runProjectEdit (working, context, "track-pan", [&] (ProjectSession& session, EditOutcome& outcome)
    {
        auto& trackContainer = *session->getAudioTrackContainer();

        auto track = findTrackById (trackContainer, trackValue.getIntValue());
        if (track == nullptr)
            return context.fail (exitFailure, "track_not_found",
                                 "no track with id " + trackValue.toStdString());

        auto numChannels = static_cast<int> (track->audioChannelContainer->objects.size());
        if (numChannels == 0)
            return context.fail (exitFailure, "no_channels",
                                 "track " + trackValue.toStdString() + " has no audio channels");

        // one channel strip, or every strip of the track (a mono stem has one)
        if (channelValue.isNotEmpty()) {
            auto channel = channelValue.getIntValue();
            if (channel < 0 || channel >= numChannels)
                return context.fail (exitUsage, "usage",
                                     "--channel must be 0.." + std::to_string (numChannels - 1));
            track->setPan (static_cast<float> (pan), channel);
        }
        else {
            for (int channel = 0; channel < numChannels; ++channel)
                track->setPan (static_cast<float> (pan), channel);
        }

        auto pans = nlohmann::json::array();
        for (int channel = 0; channel < numChannels; ++channel)
            pans.push_back (track->getPan (channel));

        outcome.log = "pan set on track " + juce::String (track->getId());
        outcome.result = { { "track", track->getId() },
                           { "pans", pans } };
        return exitOk;
    });
}

} // namespace cli
} // namespace audium
