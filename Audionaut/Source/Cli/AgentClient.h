//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Cli/CliContext.h"

namespace audium {
namespace cli {
namespace agent {

/** What the router decided about a verb. */
struct RouteOutcome
{
    /** True when the verb was answered here - by the host, or by refusing.
        The caller must return `exitCode` and not run the verb locally. */
    bool handled = false;
    int exitCode = 0;
};

/**
 * @brief Hands `args` to the app holding the project, if one holds it.
 *
 * Three outcomes, and the third is the point of the whole exercise:
 *
 * - No live host for this project: not handled, so the verb runs against the
 *   file exactly as before.
 * - A host answers: its result is rendered through `context` and returned, so
 *   hosted output is indistinguishable from a local run.
 * - A host is there but could not be reached, or speaks a protocol we do not:
 *   handled, and **refused**. Falling back to the file here would write over
 *   the document the user has open, which is what this exists to prevent.
 */
RouteOutcome routeCommand (const juce::ArgumentList& args,
                           const juce::String& verb,
                           bool verbIsHostable,
                           CliContext& context);

/** @brief Whether this verb can run against a document held by the app. */
bool isHostableVerb (const juce::String& verb);

} // namespace agent
} // namespace cli
} // namespace audium
