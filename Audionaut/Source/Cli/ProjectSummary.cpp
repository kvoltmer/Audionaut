//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/ProjectSummary.h"

#include "Engine/Channel/AudioChannel.h"
#include "Engine/Effects/Hosting/HostedEffect.h"
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

nlohmann::json describeInsert (const EffectSlot& slot, int index)
{
    const auto& effect = slot.getEffect();
    nlohmann::json insert = { { "index", index },
                              { "type", effect.getType().id },
                              { "name", effect.getType().name },
                              { "enabled", slot.isEnabled() } };

    if (auto* hosted = dynamic_cast<const HostedEffect*> (&effect))
    {
        // a plugin: its state is an opaque chunk, its parameters are listed
        // by list-effects --type; what identifies it goes out instead
        insert["plugin"] = hosted->getPluginInfo();
        insert["loaded"] = hosted->isLoaded();
        insert["status"] = hosted->getStatus() == HostedEffect::Status::loaded ? "loaded"
                         : hosted->getStatus() == HostedEffect::Status::loading ? "loading"
                         : hosted->getStatus() == HostedEffect::Status::incompatibleLayout ? "incompatible_layout"
                                                                                           : "not_loaded";
        if (hosted->getError().isNotEmpty())
            insert["error"] = hosted->getError().toStdString();
    }
    else
    {
        insert["state"] = effect.getState();
    }
    return insert;
}

nlohmann::json describeInserts (const InsertChain& chain)
{
    auto inserts = nlohmann::json::array();
    for (auto i = 0; i < chain.size(); ++i)
        inserts.push_back (describeInsert (*chain.getSlot (i), i));

    // elements the engine could make nothing of (a plugin this machine lacks):
    // listed without an index, so the slot indices stay what set-insert takes
    for (const auto& entry : chain.getUnresolved())
    {
        nlohmann::json missing = { { "missing", true }, { "position", entry.index } };
        if (entry.element.is_object())
        {
            for (const auto* key : { "type", "plugin", "enabled" })
                if (entry.element.contains (key))
                    missing[key] = entry.element[key];
        }
        inserts.push_back (std::move (missing));
    }
    return inserts;
}

nlohmann::json describeSend (int auxId, const SendState& state)
{
    return { { "aux", auxId },
             { "level", state.level.load (std::memory_order_relaxed) },
             { "preFader", state.preFader.load (std::memory_order_relaxed) },
             { "mute", state.mute.load (std::memory_order_relaxed) } };
}

nlohmann::json describeAux (const AuxBus& bus)
{
    return { { "id", bus.getId() },
             { "name", bus.getName() },
             { "gain", bus.getGain() },
             { "pan", bus.getPan() },
             { "mute", bus.getMute() },
             { "solo", bus.getSolo() },
             { "inserts", describeInserts (bus.getInserts()) } };
}

nlohmann::json describeAuxBuses (const AuxBusContainer& buses)
{
    auto array = nlohmann::json::array();
    for (const auto& bus : buses.getBuses())
        array.push_back (describeAux (*bus));
    return array;
}

nlohmann::json makeProjectSummary (AudiumEngine& engine)
{
    auto trackContainer = engine.getAudioTrackContainer();

    nlohmann::json summary;
    summary["projectFile"] = engine.getProjectFileStore()->getCurrentProjectFile().getFullPathName().toStdString();
    summary["tempoBpm"] = trackContainer->getTempoProvider()->getTempo();
    summary["masterGain"] = trackContainer->getMasterGain();
    summary["masterPan"] = trackContainer->getMasterPan();
    summary["masterMute"] = trackContainer->getMasterMute();

    auto tracks = nlohmann::json::array();
    for (auto& track : trackContainer->getAudioTracks()) {

        nlohmann::json trackJson;
        trackJson["id"] = track->getId();
        trackJson["name"] = track->getAudioTrackName().toStdString();
        auto numChannels = static_cast<int> (track->audioChannelContainer->objects.size());
        trackJson["numChannels"] = numChannels;

        // the channel strips' mixer state an agent can set (track-pan,
        // set-send): pan and sends; the inserts are the track's
        auto channels = nlohmann::json::array();
        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto sends = nlohmann::json::array();
            for (const auto& send : track->getChannel (channel)->sends.getEntries())
                sends.push_back (describeSend (send.auxId, *send.state));
            channels.push_back ({ { "pan", track->getPan (channel) },
                                  { "sends", sends } });
        }
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
    summary["auxBuses"] = describeAuxBuses (trackContainer->getAuxBuses());
    return summary;
}

} // namespace cli
} // namespace audium
