//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/ProjectSummary.h"
#include "Engine/ProjectFileStore.h"

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
        trackJson["numChannels"] = static_cast<int> (track->audioChannelContainer->objects.size());

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
    return summary;
}

} // namespace cli
} // namespace audium
