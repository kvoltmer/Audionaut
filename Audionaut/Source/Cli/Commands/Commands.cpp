//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"

#include "Engine/AudiumEngine.h"

namespace audium {
namespace cli {

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

    if (index + 1 < args.arguments.size()
        && ! args.arguments.getReference (index + 1).isOption()) {
        auto value = args.arguments.getReference (index + 1).text;
        args.arguments.removeRange (index, 2);
        return value;
    }

    args.arguments.remove (index);
    return {};
}

juce::File resolveProjectFile (const juce::ArgumentList& args, int argumentIndex)
{
    auto plain = getPlainArguments (args);
    if (argumentIndex >= plain.size())
        return {};

    auto file = juce::File::getCurrentWorkingDirectory().getChildFile (plain[argumentIndex]);

    // the .audium document package: point at the Project.json inside it
    if (AudiumEngine::isValidProjectStructure (file))
        return file.getChildFile (AudiumEngine::projectFileName);

    // a project file given directly (Project.json, or a legacy single-file .audium)
    if (AudiumEngine::isJsonProjectFile (file))
        return file;

    return {};
}

} // namespace cli
} // namespace audium
