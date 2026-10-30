//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Cli/CommandSession.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Effects/EffectRegistry.h"
#include "Engine/Effects/InsertChain.h"
#include "Engine/Effects/MixerPlanner.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Undo/UndoableEdit.h"
#include "Engine/Undo/UndoableEffectParameterAction.h"

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

/// One slot as the verbs report it: its index, type, enabled flag and state.
nlohmann::json describeSlot (const EffectSlot& slot, int index)
{
    return { { "index", index },
             { "type", slot.getEffect().getType().id },
             { "name", slot.getEffect().getType().name },
             { "enabled", slot.isEnabled() },
             { "state", slot.getEffect().getState() } };
}

nlohmann::json describeChain (const InsertChain& chain)
{
    auto inserts = nlohmann::json::array();
    for (auto i = 0; i < chain.size(); ++i)
        inserts.push_back (describeSlot (*chain.getSlot (i), i));
    return inserts;
}

/// A chain the options name, plus how the result refers to it.
struct ChainTarget
{
    InsertChain* chain = nullptr;
    InsertChainAddress address;
    nlohmann::json where;   // {"track": N, "channel": C} / {"track": N} / {"master": true}
};

/**
 * Resolves `--master`, `--track N` (the track's inserts) or `--track N
 * --channel C` (a channel's) to a chain. With allChannels, every channel of
 * the track is returned. Reports a usage or lookup failure through the
 * context and returns an empty vector.
 */
std::vector<ChainTarget> resolveTargets (CliContext& context, AudioTrackContainer& container,
                                         const juce::String& trackValue, const juce::String& channelValue,
                                         bool master, bool allChannels, int& failure)
{
    std::vector<ChainTarget> targets;
    failure = exitOk;

    if (master == trackValue.isNotEmpty())
    {
        failure = context.fail (exitUsage, "usage", "name the chain: --master, or --track <id> with or without --channel <index>");
        return {};
    }

    if (master)
    {
        targets.push_back ({ &container.getMasterInserts(), InsertChainAddress::ofMaster(), { { "master", true } } });
        return targets;
    }

    const auto trackId = trackValue.getIntValue();
    auto track = trackById (container, trackId);
    if (track == nullptr)
    {
        failure = context.fail (exitFailure, "track_not_found", "no track with id " + trackValue.toStdString());
        return {};
    }

    const auto numChannels = static_cast<int> (track->audioChannelContainer->objects.size());

    if (allChannels)
    {
        for (auto c = 0; c < numChannels; ++c)
            targets.push_back ({ &track->getChannel (c)->inserts, InsertChainAddress::ofChannel (trackId, c),
                                 { { "track", trackId }, { "channel", c } } });
        if (targets.empty())
            failure = context.fail (exitFailure, "no_channels", "track " + trackValue.toStdString() + " has no audio channels");
        return targets;
    }

    if (channelValue.isEmpty())
    {
        targets.push_back ({ &track->getTrackInserts(), InsertChainAddress::ofTrack (trackId), { { "track", trackId } } });
        return targets;
    }

    const auto channel = channelValue.getIntValue();
    if (channel < 0 || channel >= numChannels || ! channelValue.containsAnyOf ("0123456789"))
    {
        failure = context.fail (exitUsage, "usage", "--channel must be 0.." + std::to_string (numChannels - 1));
        return {};
    }

    targets.push_back ({ &track->getChannel (channel)->inserts, InsertChainAddress::ofChannel (trackId, channel),
                         { { "track", trackId }, { "channel", channel } } });
    return targets;
}

/// Takes a flag option (present or not) out of the list.
bool takeFlag (juce::ArgumentList& args, juce::StringRef option)
{
    const auto present = args.containsOption (option);
    if (present)
        args.removeOptionIfFound (option);
    return present;
}

} // namespace

int runAddInsert (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto trackValue = takeOptionValue (working, "--track");
    auto channelValue = takeOptionValue (working, "--channel");
    auto typeValue = takeOptionValue (working, "--type");
    auto atValue = takeOptionValue (working, "--at");
    const auto master = takeFlag (working, "--master");
    const auto allChannels = takeFlag (working, "--all-channels");

    if (typeValue.isEmpty())
        return context.fail (exitUsage, "usage", "add-insert requires --type <effect type id> (see list-effects)");
    if (allChannels && (trackValue.isEmpty() || channelValue.isNotEmpty()))
        return context.fail (exitUsage, "usage", "--all-channels goes with --track <id> and without --channel");

    return runProjectEdit (working, context, "add-insert", [&] (ProjectSession& session, EditOutcome& outcome)
    {
        auto& container = *session->getAudioTrackContainer();
        auto* planner = container.getMixerPlanner();
        if (planner == nullptr || ! planner->getRegistry().hasType (typeValue.toStdString()))
            return context.fail (exitFailure, "unknown_effect", "no effect of type " + typeValue.toStdString() + " (see list-effects)");

        int failure = exitOk;
        auto targets = resolveTargets (context, container, trackValue, channelValue, master, allChannels, failure);
        if (targets.empty())
            return failure;

        const auto at = atValue.isEmpty() ? -1 : atValue.getIntValue();
        auto added = nlohmann::json::array();

        const auto applied = applyAsUndoableEdit (container, [&]
        {
            for (auto& target : targets)
            {
                auto slot = target.chain->add (typeValue.toStdString(), at);
                if (slot == nullptr)
                    return false;
                auto entry = target.where;
                entry["index"] = at < 0 || at > target.chain->size() - 1 ? target.chain->size() - 1 : at;
                entry["inserts"] = describeChain (*target.chain);
                added.push_back (entry);
            }
            return true;
        }, "Add Insert", false);

        if (! applied)
            return context.fail (exitFailure, "add_failed", "could not add the insert");

        outcome.log = "added " + typeValue + " to " + juce::String (static_cast<int> (added.size())) + " chain(s)";
        outcome.result = { { "type", typeValue.toStdString() }, { "added", added } };
        return exitOk;
    });
}

int runRemoveInsert (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto trackValue = takeOptionValue (working, "--track");
    auto channelValue = takeOptionValue (working, "--channel");
    auto indexValue = takeOptionValue (working, "--index");
    const auto master = takeFlag (working, "--master");

    if (indexValue.isEmpty())
        return context.fail (exitUsage, "usage", "remove-insert requires --index <slot index>");

    return runProjectEdit (working, context, "remove-insert", [&] (ProjectSession& session, EditOutcome& outcome)
    {
        auto& container = *session->getAudioTrackContainer();
        int failure = exitOk;
        auto targets = resolveTargets (context, container, trackValue, channelValue, master, false, failure);
        if (targets.empty())
            return failure;

        auto& target = targets.front();
        const auto index = indexValue.getIntValue();
        if (index < 0 || index >= target.chain->size())
            return context.fail (exitUsage, "usage", "--index must be 0.." + std::to_string (target.chain->size() - 1));

        const auto removedType = target.chain->getSlot (index)->getEffect().getType().id;
        if (! applyAsUndoableEdit (container, [&] { return target.chain->remove (index); }, "Remove Insert", false))
            return context.fail (exitFailure, "remove_failed", "could not remove the insert");

        outcome.log = "removed " + juce::String (removedType) + " at " + juce::String (index);
        outcome.result = target.where;
        outcome.result["removed"] = { { "index", index }, { "type", removedType } };
        outcome.result["inserts"] = describeChain (*target.chain);
        return exitOk;
    });
}

int runSetInsert (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto trackValue = takeOptionValue (working, "--track");
    auto channelValue = takeOptionValue (working, "--channel");
    auto indexValue = takeOptionValue (working, "--index");
    auto paramValue = takeOptionValue (working, "--param");
    auto valueValue = takeOptionValue (working, "--value");
    auto enabledValue = takeOptionValue (working, "--enabled");
    const auto master = takeFlag (working, "--master");

    if (indexValue.isEmpty())
        return context.fail (exitUsage, "usage", "set-insert requires --index <slot index>");
    if (paramValue.isNotEmpty() != valueValue.isNotEmpty())
        return context.fail (exitUsage, "usage", "--param and --value go together");
    if (paramValue.isEmpty() && enabledValue.isEmpty())
        return context.fail (exitUsage, "usage", "set-insert takes --param <id> --value <number>, and/or --enabled on|off");
    if (enabledValue.isNotEmpty() && enabledValue != "on" && enabledValue != "off")
        return context.fail (exitUsage, "usage", "--enabled takes on or off");

    return runProjectEdit (working, context, "set-insert", [&] (ProjectSession& session, EditOutcome& outcome)
    {
        auto& container = *session->getAudioTrackContainer();
        int failure = exitOk;
        auto targets = resolveTargets (context, container, trackValue, channelValue, master, false, failure);
        if (targets.empty())
            return failure;

        auto& target = targets.front();
        const auto index = indexValue.getIntValue();
        auto slot = target.chain->getSlot (index);
        if (slot == nullptr)
            return context.fail (exitUsage, "usage", "--index must be 0.." + std::to_string (target.chain->size() - 1));

        auto& effect = slot->getEffect();
        auto undoManager = container.getUndoManager();

        if (paramValue.isNotEmpty())
        {
            const auto parameter = effect.getParameterIndex (paramValue.toStdString());
            if (parameter < 0)
                return context.fail (exitFailure, "unknown_parameter",
                                     effect.getType().name + " has no parameter " + paramValue.toStdString());
            if (! valueValue.containsAnyOf ("0123456789"))
                return context.fail (exitUsage, "usage", "--value must be a number");

            // the value as the effect will hold it: clamped to the range
            const auto oldValue = effect.getParameter (parameter);
            effect.setParameter (parameter, static_cast<float> (valueValue.getDoubleValue()));
            const auto newValue = effect.getParameter (parameter);
            effect.setParameter (parameter, oldValue);

            undoManager->perform (new UndoableEffectParameterAction (container, target.address, index, parameter, oldValue, newValue),
                                  "Set " + effect.getType().name + " " + paramValue.toStdString());
            undoManager->beginNewTransaction();
        }

        if (enabledValue.isNotEmpty())
        {
            const auto enabled = enabledValue == "on";
            if (! applyAsUndoableEdit (container, [&] { return target.chain->setEnabled (index, enabled); },
                                       enabled ? "Enable Insert" : "Disable Insert", false))
                return context.fail (exitFailure, "set_failed", "could not change the insert");
        }

        outcome.log = "set " + juce::String (effect.getType().name) + " at " + juce::String (index);
        outcome.result = target.where;
        outcome.result["insert"] = describeSlot (*slot, index);
        return exitOk;
    });
}

int runListEffects (const juce::ArgumentList& args, CliContext& context)
{
    juce::ignoreUnused (args);
    ScopedCoutToStderr guard (context.json);

    // the types are the engine's, not a project's: a bare engine answers
    auto session = openEngineSession();
    auto* planner = session->getAudioTrackContainer()->getMixerPlanner();
    if (planner == nullptr)
        return context.fail (exitFailure, "no_effects", "this engine has no effect registry");

    auto types = nlohmann::json::array();
    for (const auto& type : planner->getRegistry().getTypes())
    {
        auto effect = planner->getRegistry().create (type.id);
        auto parameters = nlohmann::json::array();
        for (const auto& info : effect->getParameters())
        {
            parameters.push_back ({ { "id", info.id },
                                    { "name", info.name },
                                    { "unit", info.unit },
                                    { "min", info.min },
                                    { "max", info.max },
                                    { "default", info.def },
                                    { "boolean", info.boolean } });
        }
        types.push_back ({ { "type", type.id }, { "name", type.name }, { "parameters", parameters } });
    }

    context.log (juce::String (static_cast<int> (types.size())) + " effect types");
    return context.ok ({ { "effects", types } });
}

} // namespace cli
} // namespace audium
