//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Cli/CommandSession.h"
#include "Cli/ProjectSummary.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Effects/EffectRegistry.h"
#include "Engine/Effects/Hosting/PluginHost.h"
#include "Engine/Effects/Hosting/PluginTypeId.h"
#include "Engine/Effects/InsertChain.h"
#include "Engine/Effects/MixerPlanner.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Undo/UndoableEdit.h"
#include "Engine/Undo/UndoableEffectParameterAction.h"

#include <limits>

namespace audium {
namespace cli {

namespace {

/// A chain the options name, plus how the result refers to it.
struct ChainTarget
{
    InsertChain* chain = nullptr;
    InsertChainAddress address;
    nlohmann::json where;   // {"track": N} / {"aux": A} / {"master": true}
};

/**
 * Resolves `--master`, `--aux ID` (an aux bus's inserts) or `--track N` (the
 * track's inserts) to a chain. Reports a usage or lookup failure through the
 * context and returns false.
 */
bool resolveTarget (CliContext& context, AudioTrackContainer& container,
                    const juce::String& trackValue, const juce::String& auxValue, bool master,
                    ChainTarget& target, int& failure)
{
    failure = exitOk;

    const auto named = (master ? 1 : 0) + (trackValue.isNotEmpty() ? 1 : 0) + (auxValue.isNotEmpty() ? 1 : 0);
    if (named != 1)
    {
        failure = context.fail (exitUsage, "usage", "name the chain: --master, --aux <id> or --track <id>");
        return false;
    }

    if (master)
    {
        target = { &container.getMasterInserts(), InsertChainAddress::ofMaster(), { { "master", true } } };
        return true;
    }

    if (auxValue.isNotEmpty())
    {
        if (! isIndexValue (auxValue))
        {
            failure = context.fail (exitUsage, "usage", "--aux must be an aux bus id (a number from 1, as info lists them)");
            return false;
        }
        const auto auxId = auxValue.getIntValue();
        auto bus = container.getAuxBuses().get (auxId);
        if (bus == nullptr)
        {
            failure = context.fail (exitFailure, "aux_not_found", "no aux bus with id " + auxValue.toStdString());
            return false;
        }
        target = { &bus->getInserts(), InsertChainAddress::ofAux (auxId), { { "aux", auxId } } };
        return true;
    }

    if (! isIndexValue (trackValue))
    {
        failure = context.fail (exitUsage, "usage", "--track must be a track id (a number from 0)");
        return false;
    }

    const auto trackId = trackValue.getIntValue();
    auto track = findTrackById (container, trackId);
    if (track == nullptr)
    {
        failure = context.fail (exitFailure, "track_not_found", "no track with id " + trackValue.toStdString());
        return false;
    }

    target = { &track->getTrackInserts(), InsertChainAddress::ofTrack (trackId), { { "track", trackId } } };
    return true;
}

/// Inserts are the track's, at its channel count: a channel option would otherwise be ignored and land on the track's chain.
bool namesAChannel (const juce::ArgumentList& args)
{
    return args.containsOption ("--channel") || args.containsOption ("--all-channels");
}

constexpr auto channelOptionMessage = "inserts belong to the track and process all its channels together: drop --channel / --all-channels";

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
    auto auxValue = takeOptionValue (working, "--aux");
    auto typeValue = takeOptionValue (working, "--type");
    auto atValue = takeOptionValue (working, "--at");
    const auto master = takeFlag (working, "--master");

    if (namesAChannel (working))
        return context.fail (exitUsage, "usage", channelOptionMessage);
    if (typeValue.isEmpty())
        return context.fail (exitUsage, "usage", "add-insert requires --type <effect type id> (see list-effects)");
    if (atValue.isNotEmpty() && ! isIndexValue (atValue))
        return context.fail (exitUsage, "usage", "--at must be a slot position (a number from 0)");

    return runProjectEdit (working, context, "add-insert", [&] (ProjectSession& session, EditOutcome& outcome)
    {
        auto& container = *session->getAudioTrackContainer();
        auto* planner = container.getMixerPlanner();
        if (planner == nullptr || ! planner->getRegistry().hasType (typeValue.toStdString()))
            return context.fail (exitFailure, "unknown_effect", "no effect of type " + typeValue.toStdString() + " (see list-effects)");

        int failure = exitOk;
        ChainTarget target;
        if (! resolveTarget (context, container, trackValue, auxValue, master, target, failure))
            return failure;

        const auto at = atValue.isEmpty() ? -1 : atValue.getIntValue();
        const auto applied = applyAsUndoableEdit (container, [&] { return target.chain->add (typeValue.toStdString(), at) != nullptr; },
                                                  "Add Insert", false);
        if (! applied)
            return context.fail (exitFailure, "add_failed", "could not add the insert");

        const auto index = at < 0 || at > target.chain->size() - 1 ? target.chain->size() - 1 : at;
        outcome.log = "added " + typeValue + " at " + juce::String (index);
        outcome.result = target.where;
        outcome.result["type"] = typeValue.toStdString();
        outcome.result["index"] = index;
        outcome.result["inserts"] = describeInserts (*target.chain);
        return exitOk;
    });
}

int runRemoveInsert (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto trackValue = takeOptionValue (working, "--track");
    auto auxValue = takeOptionValue (working, "--aux");
    auto indexValue = takeOptionValue (working, "--index");
    const auto master = takeFlag (working, "--master");

    if (namesAChannel (working))
        return context.fail (exitUsage, "usage", channelOptionMessage);
    if (indexValue.isEmpty())
        return context.fail (exitUsage, "usage", "remove-insert requires --index <slot index>");
    if (! isIndexValue (indexValue))
        return context.fail (exitUsage, "usage", "--index must be a slot index (a number from 0)");

    return runProjectEdit (working, context, "remove-insert", [&] (ProjectSession& session, EditOutcome& outcome)
    {
        auto& container = *session->getAudioTrackContainer();
        int failure = exitOk;
        ChainTarget target;
        if (! resolveTarget (context, container, trackValue, auxValue, master, target, failure))
            return failure;

        const auto index = indexValue.getIntValue();
        if (index < 0 || index >= target.chain->size())
            return context.fail (exitUsage, "usage", "--index must be 0.." + std::to_string (target.chain->size() - 1));

        const auto removedType = target.chain->getSlot (index)->getEffect().getType().id;
        if (! applyAsUndoableEdit (container, [&] { return target.chain->remove (index); }, "Remove Insert", false))
            return context.fail (exitFailure, "remove_failed", "could not remove the insert");

        outcome.log = "removed " + juce::String (removedType) + " at " + juce::String (index);
        outcome.result = target.where;
        outcome.result["removed"] = { { "index", index }, { "type", removedType } };
        outcome.result["inserts"] = describeInserts (*target.chain);
        return exitOk;
    });
}

int runSetInsert (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto trackValue = takeOptionValue (working, "--track");
    auto auxValue = takeOptionValue (working, "--aux");
    auto indexValue = takeOptionValue (working, "--index");
    auto paramValue = takeOptionValue (working, "--param");
    auto valueValue = takeOptionValue (working, "--value");
    auto enabledValue = takeOptionValue (working, "--enabled");
    const auto master = takeFlag (working, "--master");

    if (namesAChannel (working))
        return context.fail (exitUsage, "usage", channelOptionMessage);
    if (indexValue.isEmpty())
        return context.fail (exitUsage, "usage", "set-insert requires --index <slot index>");
    if (! isIndexValue (indexValue))
        return context.fail (exitUsage, "usage", "--index must be a slot index (a number from 0)");
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
        ChainTarget target;
        if (! resolveTarget (context, container, trackValue, auxValue, master, target, failure))
            return failure;

        const auto index = indexValue.getIntValue();
        auto slot = target.chain->getSlot (index);
        if (slot == nullptr)
            return context.fail (exitUsage, "usage", "--index must be 0.." + std::to_string (target.chain->size() - 1));

        auto& effect = slot->getEffect();
        auto undoManager = container.getUndoManager();

        // one verb, one undo step: the parameter and the enabled flag go
        // into the same transaction
        undoManager->beginNewTransaction();

        if (paramValue.isNotEmpty())
        {
            const auto parameter = effect.getParameterIndex (paramValue.toStdString());
            if (parameter < 0)
                return context.fail (exitFailure, "unknown_parameter",
                                     effect.getType().name + " has no parameter " + paramValue.toStdString());
            // parsed once, so what was checked is what is applied ("0x10" is a usage error, not 0)
            double value = 0.0;
            std::string error;
            if (! parseNumericOption ("--value", valueValue, std::numeric_limits<double>::lowest(), false, value, error))
                return context.fail (exitUsage, "usage", "--value must be a number");

            // the value as the effect will hold it: clamped to the range
            const auto oldValue = effect.getParameter (parameter);
            effect.setParameter (parameter, static_cast<float> (value));
            const auto newValue = effect.getParameter (parameter);
            effect.setParameter (parameter, oldValue);

            undoManager->perform (new UndoableEffectParameterAction (container, target.address, index, parameter, oldValue, newValue),
                                  "Set " + effect.getType().name + " " + paramValue.toStdString());
        }

        if (enabledValue.isNotEmpty())
        {
            // joins the transaction above (applyAsUndoableEdit only begins a new one after its perform)
            const auto enabled = enabledValue == "on";
            if (! applyAsUndoableEdit (container, [&] { return target.chain->setEnabled (index, enabled); },
                                       enabled ? "Enable Insert" : "Disable Insert", false))
                return context.fail (exitFailure, "set_failed", "could not change the insert");
        }
        else
        {
            undoManager->beginNewTransaction();
        }

        outcome.log = "set " + juce::String (effect.getType().name) + " at " + juce::String (index);
        outcome.result = target.where;
        outcome.result["insert"] = describeInsert (*slot, index);
        return exitOk;
    });
}

namespace
{
    /// The parameters of one effect instance, as list-effects reports them.
    nlohmann::json describeParameters (const Effect& effect)
    {
        auto parameters = nlohmann::json::array();
        for (const auto& info : effect.getParameters())
        {
            const auto index = static_cast<int> (&info - effect.getParameters().data());
            parameters.push_back ({ { "id", info.id },
                                    { "name", info.name },
                                    { "unit", info.unit },
                                    { "min", info.min },
                                    { "max", info.max },
                                    { "default", info.def },
                                    { "boolean", info.boolean },
                                    // the effect's own reading of the default, where it has one ("0.00 dB")
                                    { "default_text", effect.getParameterText (index, info.def).toStdString() } });
        }
        return parameters;
    }
}

int runListEffects (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    const auto typeFilter = takeOptionValue (working, "--type").toStdString();
    ScopedCoutToStderr guard (context.json);

    // the types are the engine's, not a project's: a bare engine answers
    auto session = openEngineSession (context);
    auto* planner = session->getAudioTrackContainer()->getMixerPlanner();
    if (planner == nullptr)
        return context.fail (exitFailure, "no_effects", "this engine has no effect registry");

    const auto& registry = planner->getRegistry();
    if (! typeFilter.empty() && ! registry.hasType (typeFilter))
        return context.fail (exitFailure, "unknown_effect", "no effect type " + typeFilter);

    auto types = nlohmann::json::array();
    for (const auto& type : registry.getTypes())
    {
        if (! typeFilter.empty() && type.id != typeFilter)
            continue;

        nlohmann::json entry = { { "type", type.id },
                                 { "name", type.name },
                                 { "category", type.category },
                                 { "description", type.description } };

        // a plugin is named without loading it - every plugin of the list
        // would be instantiated otherwise; --type loads the one asked for
        const auto plugin = context.pluginHost != nullptr ? context.pluginHost->findByTypeId (type.id) : std::nullopt;
        if (plugin.has_value())
        {
            entry["format"] = plugin->pluginFormatName.toStdString();
            entry["manufacturer"] = plugin->manufacturerName.toStdString();
            entry["file"] = plugin->fileOrIdentifier.toStdString();
            entry["version"] = plugin->version.toStdString();
        }

        if (! plugin.has_value() || ! typeFilter.empty())
        {
            auto effect = registry.create (type.id);
            if (effect != nullptr)
                entry["parameters"] = describeParameters (*effect);
        }
        types.push_back (std::move (entry));
    }

    context.log (juce::String (static_cast<int> (types.size())) + " effect types");
    return context.ok ({ { "effects", types } });
}

} // namespace cli
} // namespace audium
