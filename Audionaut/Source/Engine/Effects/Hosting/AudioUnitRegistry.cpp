//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

// before JUCE, whose names (Point, AudioBuffer) the system's headers would stumble over
#if defined (__APPLE__)
 #include <TargetConditionals.h>
 #if TARGET_OS_OSX
  #include <AudioToolbox/AudioToolbox.h>
 #endif
#endif

#include "AudioUnitRegistry.h"

namespace audium
{

#if JUCE_MAC && JUCE_PLUGINHOST_AU
namespace
{
    /// A four-character code as the identifiers write it.
    OSType codeOf (juce::String text)
    {
        if (text.trim().length() >= 4)   // leading spaces are part of a shorter code
            text = text.trim();
        text += "    ";
        return (static_cast<OSType> (static_cast<unsigned char> (text[0])) << 24)
             | (static_cast<OSType> (static_cast<unsigned char> (text[1])) << 16)
             | (static_cast<OSType> (static_cast<unsigned char> (text[2])) << 8)
             |  static_cast<OSType> (static_cast<unsigned char> (text[3]));
    }

    const char* categoryOf (OSType type)
    {
        switch (type)
        {
            case kAudioUnitType_Effect:
            case kAudioUnitType_MusicEffect:    return "Effect";
            case kAudioUnitType_MusicDevice:    return "Synth";
            case kAudioUnitType_Generator:      return "Generator";
            case kAudioUnitType_Panner:         return "Panner";
            case kAudioUnitType_Mixer:          return "Mixer";
            case kAudioUnitType_MIDIProcessor:  return "MidiEffects";
            default:                            return "";
        }
    }

    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wnullable-to-nonnull-conversion")   // the component is checked

    /// The component an identifier ("AudioUnit:Effects/aufx,subt,manu") names, with its three codes as
    /// the identifier writes them and as the system has them; nullptr when there is none.
    AudioComponent find (const juce::String& identifier, juce::StringArray& codes, AudioComponentDescription& wanted)
    {
        codes.clear();
        codes.addTokens (identifier.substring (juce::jmax (identifier.lastIndexOfChar (':'), identifier.lastIndexOfChar ('/')) + 1), ",", {});
        codes.removeEmptyStrings();
        if (codes.size() != 3)
            return nullptr;

        wanted = {};
        wanted.componentType = codeOf (codes[0]);
        wanted.componentSubType = codeOf (codes[1]);
        wanted.componentManufacturer = codeOf (codes[2]);
        return AudioComponentFindNext (nullptr, &wanted);
    }

    /// What the system says of the component an identifier names, in the words the format uses for
    /// the loaded plugin; nothing when there is none.
    std::optional<juce::PluginDescription> describe (const juce::String& formatName, const juce::String& identifier)
    {
        juce::StringArray codes;
        AudioComponentDescription wanted {};
        const auto component = find (identifier, codes, wanted);
        if (component == nullptr)
            return std::nullopt;

        juce::PluginDescription description;
        CFStringRef fullName = nullptr;
        if (AudioComponentCopyName (component, &fullName) == noErr && fullName != nullptr)
        {
            description.name = juce::String::fromCFString (fullName);
            CFRelease (fullName);
        }
        if (description.name.containsChar (':'))   // "manufacturer: name"
        {
            description.manufacturerName = description.name.upToFirstOccurrenceOf (":", false, false).trim();
            description.name = description.name.fromFirstOccurrenceOf (":", false, false).trim();
        }
        if (description.name.isEmpty())
            description.name = "<Unknown>";
        if (description.manufacturerName.isEmpty())
            description.manufacturerName = codes[2];

        UInt32 version = 0;
        if (AudioComponentGetVersion (component, &version) == noErr)
            description.version << static_cast<int> (version >> 16) << "."
                                << static_cast<int> ((version >> 8) & 0xff) << "."
                                << static_cast<int> (version & 0xff);

        description.descriptiveName = description.name;
        description.fileOrIdentifier = identifier;
        description.uniqueId = description.deprecatedUid = static_cast<int> (wanted.componentType)
                                                         ^ static_cast<int> (wanted.componentSubType)
                                                         ^ static_cast<int> (wanted.componentManufacturer);
        description.pluginFormatName = formatName;
        description.category = categoryOf (wanted.componentType);
        description.isInstrument = wanted.componentType == kAudioUnitType_MusicDevice;
        description.lastFileModTime = juce::Time();
        description.lastInfoUpdateTime = juce::Time();   // not scanned
        return description;
    }

    JUCE_END_IGNORE_WARNINGS_GCC_LIKE
}
#endif

std::vector<juce::PluginDescription> AudioUnitRegistry::installed (juce::AudioPluginFormatManager& formats)
{
    std::vector<juce::PluginDescription> result;
   #if JUCE_MAC && JUCE_PLUGINHOST_AU
    for (auto* format : formats.getFormats())
    {
        if (format->getName() != juce::AudioUnitPluginFormat().getName())
            continue;

        // the identifiers a scan would go through, those that load out of process included
        for (const auto& identifier : format->searchPathsForPlugins ({}, true, true))
            if (auto description = describe (format->getName(), identifier))
                result.push_back (std::move (*description));
    }
   #endif
    juce::ignoreUnused (formats);
    return result;
}

bool AudioUnitRegistry::isSandboxSafe (const juce::String& identifier)
{
   #if JUCE_MAC && JUCE_PLUGINHOST_AU
    juce::StringArray codes;
    AudioComponentDescription wanted {}, registered {};
    const auto component = find (identifier, codes, wanted);
    if (component == nullptr || AudioComponentGetDescription (component, &registered) != noErr)
        return true;   // nothing there to open

    // a version 3 unit runs in a process of its own, whatever it says of itself
    return (registered.componentFlags & (kAudioComponentFlag_SandboxSafe | kAudioComponentFlag_IsV3AudioUnit)) != 0;
   #else
    juce::ignoreUnused (identifier);
    return true;
   #endif
}

} // namespace audium
