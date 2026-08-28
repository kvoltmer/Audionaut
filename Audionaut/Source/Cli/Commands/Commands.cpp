//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"

#include "Engine/AudiumEngine.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Provider/TempoProvider.h"

namespace audium {
namespace cli {

bool parseMusicalPosition (const juce::String& value,
                           const juce::String& unit,
                           const TempoProvider& tempoProvider,
                           double& outClocks,
                           std::string& error)
{
    const auto numeric = value.getDoubleValue();

    if (unit == "bars")
        outClocks = (numeric - 1.0) * clocksPerBar;
    else if (unit == "beats")
        outClocks = (numeric - 1.0) * clocksPerBeat;
    else if (unit == "seconds")
        outClocks = tempoProvider.secondsToClocks (numeric);
    else if (unit == "clocks")
        outClocks = numeric;
    else {
        error = "--unit must be bars, beats, seconds or clocks";
        return false;
    }

    if (outClocks < 0.0) {
        error = "position is before the start of the timeline";
        return false;
    }

    return true;
}

bool parseMusicalDuration (const juce::String& value,
                           const juce::String& unit,
                           const TempoProvider& tempoProvider,
                           double& outClocks,
                           std::string& error)
{
    const auto numeric = value.getDoubleValue();

    if (unit == "bars")
        outClocks = numeric * clocksPerBar;
    else if (unit == "beats")
        outClocks = numeric * clocksPerBeat;
    else if (unit == "seconds")
        outClocks = tempoProvider.secondsToClocks (numeric);
    else if (unit == "clocks")
        outClocks = numeric;
    else {
        error = "--unit must be bars, beats, seconds or clocks";
        return false;
    }

    if (outClocks <= 0.0) {
        error = "duration must be positive";
        return false;
    }

    return true;
}

std::vector<std::pair<std::shared_ptr<AudioTrack>, std::shared_ptr<AudioRegion>>>
findRegionsByName (const AudioTrackContainer& tracks, const juce::String& name, int trackId)
{
    std::vector<std::pair<std::shared_ptr<AudioTrack>, std::shared_ptr<AudioRegion>>> matches;

    for (auto& track : tracks.getAudioTracks()) {
        if (trackId >= 0 && track->getId() != trackId)
            continue;
        for (auto& region : track->getRegions())
            if (region->getName() == name)
                matches.emplace_back (track, region);
    }

    return matches;
}

juce::StringArray getPlainArguments (const juce::ArgumentList& args)
{
    juce::StringArray plain;

    // arguments[0] is the command word itself
    for (int i = 1; i < args.arguments.size(); ++i)
        if (! args.arguments.getReference (i).isOption())
            plain.add (args.arguments.getReference (i).text);

    return plain;
}

juce::String takeOptionValue (juce::ArgumentList& args,
                              juce::StringRef option,
                              const juce::String& defaultValue)
{
    auto index = args.indexOfOption (option);
    if (index < 0)
        return defaultValue;

    auto& argument = args.arguments.getReference (index);

    if (argument.isLongOption()) {
        auto inlineValue = argument.getLongOptionValue();
        if (inlineValue.isNotEmpty()) {
            args.arguments.remove (index);
            return inlineValue;
        }
    }

    if (index + 1 < args.arguments.size()) {
        auto& next = args.arguments.getReference (index + 1);
        // isOption() is true for any leading dash, which would swallow
        // negative numeric values ("--gain -6"); let those through
        bool negativeNumber = next.text.length() > 1 && next.text[0] == '-'
                              && (juce::CharacterFunctions::isDigit (next.text[1])
                                  || next.text[1] == '.');
        if (! next.isOption() || negativeNumber) {
            auto value = next.text;
            args.arguments.removeRange (index, 2);
            return value;
        }
    }

    args.arguments.remove (index);
    return {};
}

juce::File workingDirectory()
{
    auto pwd = juce::SystemStats::getEnvironmentVariable ("PWD", {});
    if (pwd.isNotEmpty() && juce::File::isAbsolutePath (pwd)) {
        juce::File dir (pwd);
        if (dir.isDirectory())
            return dir;
    }
    return juce::File::getCurrentWorkingDirectory();
}

juce::File resolveProjectFile (const juce::ArgumentList& args, int argumentIndex)
{
    auto plain = getPlainArguments (args);
    if (argumentIndex >= plain.size())
        return {};

    auto file = workingDirectory().getChildFile (plain[argumentIndex]);

    // the .audium document package: point at the Project.json inside it
    if (ProjectFileStore::isValidProjectStructure (file))
        return file.getChildFile (ProjectFileStore::projectFileName);

    // a project file given directly (Project.json, or a legacy single-file .audium)
    if (ProjectFileStore::isJsonProjectFile (file))
        return file;

    return {};
}

} // namespace cli
} // namespace audium
