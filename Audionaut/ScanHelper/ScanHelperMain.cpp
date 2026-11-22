//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>

#include "Cli/CliContext.h"
#include "Engine/Effects/Hosting/ChildProcessScanner.h"
#include "Engine/Effects/Hosting/ScanWorker.h"

//==============================================================================
// audionaut-scan-helper - the plugin scan worker as an executable of its own.
//
// A sandboxed macOS app cannot run its own binary as a child process: the
// child inherits the sandbox, sets out to enter the one its entitlements
// name, and the system ends it for that before main(). A child may only
// carry com.apple.security.app-sandbox and com.apple.security.inherit, so
// the app bundle brings this helper (Contents/Helpers), signed with exactly
// those two, and ChildProcessScanner starts it in the app's place.
//
// It takes the command line of the app's `scan-plugin` verb and answers the
// same way:
//
//   audionaut-scan-helper scan-plugin --serve <pipe>
//   audionaut-scan-helper scan-plugin --json [--timeout-ms N]
//                         --format F <file or identifier>
//
// Exit codes: 0 success, 1 the scan failed, 2 usage error, 3 no such format
// in this build.
//==============================================================================

namespace
{

/// Takes `--option value` out of the arguments; "" when it is not there.
juce::String takeValue (juce::StringArray& arguments, const juce::String& option)
{
    const auto index = arguments.indexOf (option);
    if (index < 0)
        return {};

    const auto value = arguments[index + 1];   // "" past the end
    arguments.removeRange (index, 2);
    return value;
}

bool takeFlag (juce::StringArray& arguments, const juce::String& flag)
{
    const auto index = arguments.indexOf (flag);
    if (index >= 0)
        arguments.remove (index);
    return index >= 0;
}

} // namespace

int main (int argc, char* argv[])
{
    using namespace audium;

    juce::StringArray arguments;
    for (auto i = 1; i < argc; ++i)
        arguments.add (juce::String::fromUTF8 (argv[i]));

    cli::CliContext context;   // captures the real stdout before the redirect
    context.json = takeFlag (arguments, "--json");
    context.quiet = takeFlag (arguments, "--quiet");
    takeFlag (arguments, "scan-plugin");   // the verb, as the app is given it

    const auto pipeName = takeValue (arguments, "--serve");
    const auto formatName = takeValue (arguments, "--format");
    const auto timeoutText = takeValue (arguments, "--timeout-ms");
    const auto timeoutMs = timeoutText.getIntValue() > 0 ? timeoutText.getIntValue() : 50000;
    cli::ScopedCoutToStderr guard (context.json || pipeName.isNotEmpty());   // whatever a plugin prints while loading stays off the envelope

    if (pipeName.isNotEmpty())
        return ScanWorker::serve (pipeName);

    if (formatName.isEmpty())
        return context.fail (cli::exitUsage, "usage", "scan-plugin requires --format <name>");
    if (arguments.isEmpty())
        return context.fail (cli::exitUsage, "usage", "scan-plugin requires the plugin file or identifier");
    const auto fileOrIdentifier = arguments[0];

    const auto answer = ScanWorker::scanOneFile ({ formatName, fileOrIdentifier, timeoutMs });
    if (! answer.value ("ok", false))
    {
        const auto code = answer.value ("code", std::string ("failed"));
        return context.fail (code == "unknown_format" ? cli::exitUnavailable : cli::exitFailure, code,
                             answer.value ("message", std::string()));
    }

    context.log (juce::String (static_cast<int> (answer["plugins"].size())) + " plugin(s) in " + fileOrIdentifier);
    return context.ok ({ { "plugins", answer["plugins"] } });
}
