//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <string>

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

#include "Engine/Effects/Effect.h"

namespace audium
{

/**
 * How a hosted plugin is named in a project: the type id an insert element
 * stores ("au:1a2b3c4d", "vst3:..", "lv2:.."), from the format and the
 * plugin's own unique id, which is the same on every machine the plugin is
 * installed on - unlike JUCE's identifier string, which carries the file
 * path. Beside the id the element gets a "plugin" block (name, manufacturer,
 * format, file, version) so a machine that lacks the plugin can say what is
 * missing, and one that has it in another format can find it.
 */
namespace PluginTypeId
{
    /// "AudioUnit" -> "au", "VST3" -> "vst3", "LV2" -> "lv2"; anything else lowercased without spaces.
    inline std::string formatKey (const juce::String& formatName)
    {
        if (formatName == "AudioUnit") return "au";
        if (formatName == "VST3")      return "vst3";
        if (formatName == "LV2")       return "lv2";
        return formatName.toLowerCase().removeCharacters (" ").toStdString();
    }

    /// The type id: format key, a colon, the unique id as eight hex digits.
    inline std::string make (const juce::PluginDescription& description)
    {
        const auto uid = description.uniqueId != 0 ? description.uniqueId : description.deprecatedUid;
        const auto hex = juce::String::toHexString (static_cast<juce::int64> (static_cast<juce::uint32> (uid)));
        return formatKey (description.pluginFormatName) + ":" + hex.paddedLeft ('0', 8).toStdString();
    }

    /// The "plugin" block of an insert element.
    inline nlohmann::json pluginInfo (const juce::PluginDescription& description)
    {
        return { { "name", description.name.toStdString() },
                 { "manufacturer", description.manufacturerName.toStdString() },
                 { "format", description.pluginFormatName.toStdString() },
                 { "file", description.fileOrIdentifier.toStdString() },
                 { "version", description.version.toStdString() } };
    }

    /// What the registry lists the plugin as: the manufacturer is the menu's category, the format shows in the description.
    inline EffectTypeInfo typeInfo (const juce::PluginDescription& description)
    {
        const auto manufacturer = description.manufacturerName.trim();
        auto detail = description.descriptiveName.isNotEmpty() && description.descriptiveName != description.name
                        ? description.descriptiveName : description.name;
        return { make (description),
                 description.name.toStdString(),
                 (manufacturer.isNotEmpty() ? manufacturer : description.pluginFormatName).toStdString(),
                 (detail + " (" + description.pluginFormatName + ")").toStdString() };
    }
}

} // namespace audium
