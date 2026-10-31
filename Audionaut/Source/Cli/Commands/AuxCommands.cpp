//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Cli/CommandSession.h"
#include "Cli/ProjectSummary.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Effects/AuxBusContainer.h"
#include "Engine/Effects/MixerPlanner.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Undo/UndoableEdit.h"
#include "Engine/Undo/UndoableSendAction.h"

#include <cmath>
#include <cstdlib>

namespace audium {
namespace cli {

namespace {

/// An id or channel number as typed: digits only, so a typo never acts on 0.
bool isIdValue (const juce::String& value)
{
    return value.isNotEmpty() && value.containsOnly ("0123456789");
}

/// A level, gain or balance as typed: one finite number and nothing else.
bool isNumberValue (const juce::String& value)
{
    const auto text = value.trim().toStdString();
    if (text.empty())
        return false;
    char* end = nullptr;
    const auto number = std::strtod (text.c_str(), &end);
    return end == text.c_str() + text.size() && std::isfinite (number);
}

/// An on/off option: empty (not given), "on" or "off"; anything else is a usage error.
bool isSwitchValue (const juce::String& value)
{
    return value.isEmpty() || value == "on" || value == "off";
}

/// A channel of a track, as the send verbs address it.
struct SendTarget
{
    std::shared_ptr<AudioChannel> channel;
    int trackId = -1;
    int channelNumber = -1;
};

/**
 * Resolves --track N with or without --channel C to the channels a send
 * verb acts on: one, or every channel of the track. Reports a usage or
 * lookup failure through the context and returns an empty vector.
 */
std::vector<SendTarget> resolveSendTargets (CliContext& context, AudioTrackContainer& container,
                                            const juce::String& trackValue, const juce::String& channelValue, int& failure)
{
    failure = exitOk;

    if (! isIdValue (trackValue))
    {
        failure = context.fail (exitUsage, "usage", "--track must be a track id (a number from 0)");
        return {};
    }

    const auto trackId = trackValue.getIntValue();
    auto track = findTrackById (container, trackId);
    if (track == nullptr)
    {
        failure = context.fail (exitFailure, "track_not_found", "no track with id " + trackValue.toStdString());
        return {};
    }

    const auto numChannels = static_cast<int> (track->audioChannelContainer->objects.size());
    std::vector<SendTarget> targets;

    if (channelValue.isEmpty())
    {
        for (auto c = 0; c < numChannels; ++c)
            targets.push_back ({ track->getChannel (c), trackId, c });
        if (targets.empty())
            failure = context.fail (exitFailure, "no_channels", "track " + trackValue.toStdString() + " has no audio channels");
        return targets;
    }

    const auto channel = isIdValue (channelValue) ? channelValue.getIntValue() : -1;
    if (channel < 0 || channel >= numChannels)
    {
        failure = context.fail (exitUsage, "usage", "--channel must be 0.." + std::to_string (numChannels - 1));
        return {};
    }

    targets.push_back ({ track->getChannel (channel), trackId, channel });
    return targets;
}

/// The aux bus --aux names, or a failure through the context.
std::shared_ptr<AuxBus> resolveAux (CliContext& context, AudioTrackContainer& container, const juce::String& auxValue, int& failure)
{
    failure = exitOk;
    if (! isIdValue (auxValue))
    {
        failure = context.fail (exitUsage, "usage", "--aux must be an aux bus id (a number from 1, as info lists them)");
        return nullptr;
    }
    auto bus = container.getAuxBuses().get (auxValue.getIntValue());
    if (bus == nullptr)
        failure = context.fail (exitFailure, "aux_not_found", "no aux bus with id " + auxValue.toStdString());
    return bus;
}

} // namespace

int runAddAux (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto nameValue = takeOptionValue (working, "--name");

    return runProjectEdit (working, context, "add-aux", [&] (ProjectSession& session, EditOutcome& outcome)
    {
        auto& container = *session->getAudioTrackContainer();
        std::shared_ptr<AuxBus> bus;

        const auto applied = applyAsUndoableEdit (container, [&]
        {
            bus = container.addAuxBus (nameValue.toStdString());
            return bus != nullptr;
        }, "Add Aux Bus", false);

        if (! applied)
            return context.fail (exitFailure, "add_failed", "could not add the aux bus");

        outcome.log = "added aux bus " + juce::String (bus->getId()) + " (" + juce::String (bus->getName()) + ")";
        outcome.result = { { "aux", describeAux (*bus) }, { "auxBuses", describeAuxBuses (container.getAuxBuses()) } };
        return exitOk;
    });
}

int runRemoveAux (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto auxValue = takeOptionValue (working, "--aux");

    if (auxValue.isEmpty())
        return context.fail (exitUsage, "usage", "remove-aux requires --aux <id>");

    return runProjectEdit (working, context, "remove-aux", [&] (ProjectSession& session, EditOutcome& outcome)
    {
        auto& container = *session->getAudioTrackContainer();
        int failure = exitOk;
        auto bus = resolveAux (context, container, auxValue, failure);
        if (bus == nullptr)
            return failure;

        const auto id = bus->getId();
        const auto name = bus->getName();
        if (! applyAsUndoableEdit (container, [&] { return container.removeAuxBus (id); }, "Remove Aux Bus", false))
            return context.fail (exitFailure, "remove_failed", "could not remove the aux bus");

        outcome.log = "removed aux bus " + juce::String (id) + " (" + juce::String (name) + ") and the sends into it";
        outcome.result = { { "removed", { { "id", id }, { "name", name } } }, { "auxBuses", describeAuxBuses (container.getAuxBuses()) } };
        return exitOk;
    });
}

int runSetAux (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto auxValue = takeOptionValue (working, "--aux");
    auto nameValue = takeOptionValue (working, "--name");
    auto gainValue = takeOptionValue (working, "--gain");
    auto balanceValue = takeOptionValue (working, "--balance");
    auto muteValue = takeOptionValue (working, "--mute");
    auto soloValue = takeOptionValue (working, "--solo");

    if (auxValue.isEmpty())
        return context.fail (exitUsage, "usage", "set-aux requires --aux <id>");
    if (nameValue.isEmpty() && gainValue.isEmpty() && balanceValue.isEmpty() && muteValue.isEmpty() && soloValue.isEmpty())
        return context.fail (exitUsage, "usage", "set-aux takes --name N, --gain G, --balance B, --mute on|off and/or --solo on|off");
    if (gainValue.isNotEmpty() && (! isNumberValue (gainValue) || gainValue.getDoubleValue() < 0.0))
        return context.fail (exitUsage, "usage", "--gain must be a linear gain from 0 (1 = unity)");
    if (balanceValue.isNotEmpty() && (! isNumberValue (balanceValue) || std::abs (balanceValue.getDoubleValue()) > 1.0))
        return context.fail (exitUsage, "usage", "--balance must be -1 (left) .. 1 (right)");
    if (! isSwitchValue (muteValue) || ! isSwitchValue (soloValue))
        return context.fail (exitUsage, "usage", "--mute and --solo take on or off");

    return runProjectEdit (working, context, "set-aux", [&] (ProjectSession& session, EditOutcome& outcome)
    {
        auto& container = *session->getAudioTrackContainer();
        int failure = exitOk;
        auto bus = resolveAux (context, container, auxValue, failure);
        if (bus == nullptr)
            return failure;

        auto undoManager = container.getUndoManager();

        // one verb, one undo step: the values go into the bus's atomics
        // (playback undisturbed), the name through a container snapshot
        undoManager->beginNewTransaction();

        const auto oldValues = UndoableAuxAction::Values::of (*bus);
        auto newValues = oldValues;
        if (gainValue.isNotEmpty())
            newValues.gain = static_cast<float> (gainValue.getDoubleValue());
        if (balanceValue.isNotEmpty())
            newValues.balance = static_cast<float> (balanceValue.getDoubleValue());
        if (muteValue.isNotEmpty())
            newValues.mute = muteValue == "on";
        if (soloValue.isNotEmpty())
            newValues.solo = soloValue == "on";

        if (gainValue.isNotEmpty() || balanceValue.isNotEmpty() || muteValue.isNotEmpty() || soloValue.isNotEmpty())
            undoManager->perform (new UndoableAuxAction (container, bus->getId(), oldValues, newValues), "Set Aux Bus");

        if (nameValue.isNotEmpty())
        {
            // joins the transaction above (applyAsUndoableEdit only begins a new one after its perform)
            if (! applyAsUndoableEdit (container, [&] { bus->setName (nameValue.toStdString()); return true; }, "Rename Aux Bus", false))
                return context.fail (exitFailure, "set_failed", "could not rename the aux bus");
        }
        else
        {
            undoManager->beginNewTransaction();
        }

        outcome.log = "set aux bus " + juce::String (bus->getId()) + " (" + juce::String (bus->getName()) + ")";
        outcome.result = { { "aux", describeAux (*bus) } };
        return exitOk;
    });
}

int runSetSend (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto trackValue = takeOptionValue (working, "--track");
    auto channelValue = takeOptionValue (working, "--channel");
    auto auxValue = takeOptionValue (working, "--aux");
    auto levelValue = takeOptionValue (working, "--level");
    auto preFaderValue = takeOptionValue (working, "--pre-fader");
    auto muteValue = takeOptionValue (working, "--mute");

    if (trackValue.isEmpty() || auxValue.isEmpty())
        return context.fail (exitUsage, "usage", "set-send requires --track <id> [--channel <index>] --aux <id>");
    if (levelValue.isEmpty() && preFaderValue.isEmpty() && muteValue.isEmpty())
        return context.fail (exitUsage, "usage", "set-send takes --level L, --pre-fader on|off and/or --mute on|off");
    if (levelValue.isNotEmpty() && (! isNumberValue (levelValue) || levelValue.getDoubleValue() < 0.0))
        return context.fail (exitUsage, "usage", "--level must be a linear gain from 0 (1 = unity)");
    if (! isSwitchValue (preFaderValue) || ! isSwitchValue (muteValue))
        return context.fail (exitUsage, "usage", "--pre-fader and --mute take on or off");

    return runProjectEdit (working, context, "set-send", [&] (ProjectSession& session, EditOutcome& outcome)
    {
        auto& container = *session->getAudioTrackContainer();
        int failure = exitOk;
        auto bus = resolveAux (context, container, auxValue, failure);
        if (bus == nullptr)
            return failure;
        auto targets = resolveSendTargets (context, container, trackValue, channelValue, failure);
        if (targets.empty())
            return failure;

        const auto auxId = bus->getId();
        auto undoManager = container.getUndoManager();
        undoManager->beginNewTransaction();

        // a send that is there changes its values in place (its atomics,
        // playback undisturbed); one that is not is made with them, through
        // a container snapshot, as it changes the plan's structure
        const auto valuesFrom = [&] (UndoableSendAction::Values values)
        {
            if (levelValue.isNotEmpty())
                values.level = static_cast<float> (levelValue.getDoubleValue());
            if (preFaderValue.isNotEmpty())
                values.preFader = preFaderValue == "on";
            if (muteValue.isNotEmpty())
                values.mute = muteValue == "on";
            return values;
        };

        std::vector<SendTarget> created;
        for (auto& target : targets)
        {
            if (auto state = target.channel->sends.get (auxId))
            {
                const auto oldValues = UndoableSendAction::Values::of (*state);
                undoManager->perform (new UndoableSendAction (container, target.trackId, target.channelNumber, auxId, oldValues, valuesFrom (oldValues)),
                                      "Set Send");
            }
            else
            {
                created.push_back (target);
            }
        }

        if (! created.empty())
        {
            // every new send in one plan, joining the transaction above
            auto* planner = container.getMixerPlanner();
            const auto applied = applyAsUndoableEdit (container, [&]
            {
                MixerPlanner::ScopedBatch planBatch (planner);
                for (auto& target : created)
                {
                    auto state = target.channel->sends.getOrAdd (auxId);
                    if (state == nullptr)
                        return false;
                    valuesFrom (UndoableSendAction::Values::of (*state)).applyTo (*state);
                }
                return true;
            }, "Set Send", false);
            if (! applied)
                return context.fail (exitFailure, "set_failed", "could not set the send");
        }
        else
        {
            undoManager->beginNewTransaction();
        }

        auto sends = nlohmann::json::array();
        for (auto& target : targets)
        {
            auto entry = describeSend (auxId, *target.channel->sends.get (auxId));
            entry["track"] = target.trackId;
            entry["channel"] = target.channelNumber;
            sends.push_back (entry);
        }

        outcome.log = "set the send into aux " + juce::String (auxId) + " on " + juce::String (static_cast<int> (targets.size())) + " channel(s)";
        outcome.result = { { "aux", auxId }, { "sends", sends } };
        return exitOk;
    });
}

int runRemoveSend (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto trackValue = takeOptionValue (working, "--track");
    auto channelValue = takeOptionValue (working, "--channel");
    auto auxValue = takeOptionValue (working, "--aux");

    if (trackValue.isEmpty() || auxValue.isEmpty())
        return context.fail (exitUsage, "usage", "remove-send requires --track <id> [--channel <index>] --aux <id>");
    if (! isIdValue (auxValue))
        return context.fail (exitUsage, "usage", "--aux must be an aux bus id (a number from 1)");

    return runProjectEdit (working, context, "remove-send", [&] (ProjectSession& session, EditOutcome& outcome)
    {
        auto& container = *session->getAudioTrackContainer();
        int failure = exitOk;
        auto targets = resolveSendTargets (context, container, trackValue, channelValue, failure);
        if (targets.empty())
            return failure;

        // the bus itself need not be there: a send into a removed bus goes too
        const auto auxId = auxValue.getIntValue();
        auto removed = nlohmann::json::array();
        auto* planner = container.getMixerPlanner();

        const auto applied = applyAsUndoableEdit (container, [&]
        {
            MixerPlanner::ScopedBatch planBatch (planner);
            for (auto& target : targets)
                if (target.channel->sends.remove (auxId))
                    removed.push_back ({ { "track", target.trackId }, { "channel", target.channelNumber } });
            return ! removed.empty();
        }, "Remove Send", false);

        if (! applied)
            return context.fail (exitFailure, "send_not_found", "no send into aux " + auxValue.toStdString() + " on the channel(s) named");

        outcome.log = "removed the send into aux " + juce::String (auxId) + " from " + juce::String (static_cast<int> (removed.size())) + " channel(s)";
        outcome.result = { { "aux", auxId }, { "removed", removed } };
        return exitOk;
    });
}

} // namespace cli
} // namespace audium
