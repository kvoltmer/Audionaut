//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <vector>
#include <JuceHeader.h>

#include "Cli/CliContext.h"

namespace audium {
namespace cli {

/**
 * Returned by performCliCommand when the arguments name no known verb, so
 * the GUI app can fall through to its normal startup (Projucer-style) while
 * the console binary reports a usage error. Deliberately outside the real
 * exit-code range.
 */
constexpr int cliCommandNotPerformed = 0x61756463;

/**
 * @struct CliCommandSpec
 * @brief One CLI verb: its name, help text, and handler.
 *
 * The single source of truth for the command set - CliMain registers its
 * ConsoleApplication commands from this table, and the GUI app's in-app CLI
 * dispatches against it.
 */
struct CliCommandSpec {
    juce::String verb;
    juce::String usage;
    juce::String shortHelp;
    juce::String longHelp;
    int (*run) (const juce::ArgumentList&, CliContext&);
};

/** The CLI's verbs, in help-listing order. */
const std::vector<CliCommandSpec>& getCliCommands();

/**
 * Dispatches args[0] against the command table.
 *
 * Returns the command's exit code, or cliCommandNotPerformed when the list
 * is empty or the first argument is not a known verb (options and file paths
 * fall through). Exceptions escaping a handler are converted to an error
 * envelope + exitFailure rather than propagating - in the GUI app this runs
 * inside initialise(), where an escaped exception would take the app down.
 */
int performCliCommand (const juce::ArgumentList& args, CliContext& context);

} // namespace cli
} // namespace audium
