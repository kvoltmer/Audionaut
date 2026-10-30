//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/ProjectSummary.h"

#include "Engine/Channel/AudioChannel.h"
#include "Engine/Effects/InsertChain.h"
#include "Engine/Project/ProjectFileStore.h"

#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Resource/AudioResource.h"

namespace audium {
namespace cli {

namespace {

/// An insert chain as the summary shows it: index, type, name, enabled and state per slot.
nlohmann::json describeInserts (const InsertChain& chain)
{
    auto inserts = nlohmann::json::array();
    for (auto i = 0; i < chain.size(); ++i) {
        const auto& slot = *chain.getSlot (i);
        inserts.push_back ({ { "index", i },
                             { "type", slot.getEffect().getType().id },
                             { "name", slot.getEffect().getType().name },
                             { "enabled", slot.isEnabled() },
                             { "state", slot.getEffect().getState() } });
    }
    return inserts;
}

} // namespace

nlohmann::json makeProjectSummary (AudiumEngine& engine)
{
    auto trackContainer = engine.getAudioTrackContainer();

    nlohmann::json summary;
    summary["projectFile"] = engine.getProjectFileStore()->getCurrentProjectFile().getFullPathName().toStdString();
    summary["tempoBpm"] = trackContainer->getTempoProvider()->getTempo();
    summary["masterGain"] = trackContainer->getMasterGain();

    auto tracks = nlohmann::json::array();
    for (auto& track : trackContainer->getAudioTracks()) {

        nlohmann::json trackJson;
        trackJson["id"] = track->getId();
        trackJson["name"] = track->getAudioTrackName().toStdString();
        auto numChannels = static_cast<int> (track->audioChannelContainer->objects.size());
        trackJson["numChannels"] = numChannels;

        // the channel strips' mixer state an agent can set (track-pan,
        // add-insert): pan and the strip's insert chain
        auto channels = nlohmann::json::array();
        for (int channel = 0; channel < numChannels; ++channel)
            channels.push_back ({ { "pan", track->getPan (channel) },
                                  { "inserts", describeInserts (track->getChannel (channel)->inserts) } });
        trackJson["channels"] = channels;
        trackJson["inserts"] = describeInserts (track->getTrackInserts());

        auto clips = nlohmann::json::array();
        for (auto& item : track->getPlayListContainer()->getPlayListItems()) {

            nlohmann::json clipJson;
            clipJson["positionSeconds"] = item->getAbsolutePosition (audium::seconds);
            clipJson["durationSeconds"] = item->getDurationTime (audium::seconds);

            if (auto region = item->getRegion()) {
                clipJson["region"] = region->getName().toStdString();

                auto files = nlohmann::json::array();
                for (auto& resource : region->getAudioResources())
                    files.push_back (resource->getUrl().getLocalFile().getFileName().toStdString());
                clipJson["files"] = files;
            }

            clips.push_back (clipJson);
        }

        trackJson["clips"] = clips;
        tracks.push_back (trackJson);
    }

    summary["tracks"] = tracks;
    summary["masterInserts"] = describeInserts (trackContainer->getMasterInserts());
    return summary;
}

} // namespace cli
} // namespace audium
