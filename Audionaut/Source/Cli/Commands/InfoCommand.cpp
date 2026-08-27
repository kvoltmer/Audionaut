//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <iostream>

#include "Cli/Commands/Commands.h"
#include "Engine/ProjectFileStore.h"
#include "Cli/HeadlessEngineSession.h"
#include "Cli/ProjectSummary.h"

namespace audium {
namespace cli {

int runInfo (const juce::ArgumentList& args, CliContext& context)
{
    auto projectFile = resolveProjectFile (args);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "info requires an existing <project.audium>");

    // Installed before the session so engine prints during construction,
    // the command and teardown all land on stderr in --json mode. The result
    // envelope is immune - CliContext captured the real stdout at startup.
    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    std::string openError;
    if (! session->getProjectFileStore()->open (projectFile, [&openError] (std::string error) { openError = error; }))
        return context.fail (exitFailure, "open_failed",
                             openError.empty() ? "failed to open project" : openError);

    nlohmann::json result;
    if (args.containsOption ("--raw")) {
        json full;
        session->writeToJson (full);
        result = full;
    }
    else {
        result = makeProjectSummary (*session.get());
    }

    if (context.json)
        return context.ok (result);

    std::cout << result.dump (2) << std::endl;
    return exitOk;
}

} // namespace cli
} // namespace audium
