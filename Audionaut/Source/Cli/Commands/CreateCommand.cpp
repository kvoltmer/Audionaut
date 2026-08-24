//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Cli/HeadlessEngineSession.h"

#include "Engine/AudiumEngine.h"

namespace audium {
namespace cli {

int runCreate (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto numChannels = takeOptionValue (working, "--channels", "2").getIntValue();
    if (numChannels < 1)
        return context.fail (exitUsage, "usage", "--channels must be at least 1");

    auto plain = getPlainArguments (working);
    if (plain.isEmpty())
        return context.fail (exitUsage, "usage", "create requires a target <project.audium> path");

    auto target = juce::File::getCurrentWorkingDirectory().getChildFile (plain[0]);
    if (! target.getFileName().endsWith (AudiumEngine::projectFileExtension))
        return context.fail (exitUsage, "usage",
                             "the project path must end with " + std::string (AudiumEngine::projectFileExtension));

    if (target.exists())
        return context.fail (exitFailure, "exists",
                             "refusing to overwrite existing " + target.getFullPathName().toStdString());

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;
    session->createNewProject (numChannels);

    auto projectFile = target.getChildFile (AudiumEngine::projectFileName);
    std::string saveError;
    if (! session->saveFile (projectFile, [&saveError] (std::string error) { saveError = error; }))
        return context.fail (exitFailure, "save_failed",
                             saveError.empty() ? "failed to save project" : saveError);

    context.log ("created " + target.getFullPathName());
    return context.ok ({ { "projectFile", projectFile.getFullPathName().toStdString() },
                         { "numChannels", numChannels } });
}

} // namespace cli
} // namespace audium
