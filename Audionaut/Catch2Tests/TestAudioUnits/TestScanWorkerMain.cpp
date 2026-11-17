//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

// before JuceHeader.h, whose `using namespace juce` makes the system's Point and AudioBuffer ambiguous
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>

#include <JuceHeader.h>

#include "Engine/Effects/Hosting/ScanWorker.h"

//==============================================================================
// The scan worker for the tests' Audio Units: the scan helper's serving form
// (`scan-plugin --serve <pipe>`, Audionaut/ScanHelper) in a process that
// first registers the components in the TestAudioUnits folder beside it.
//
// The system only lists components that are installed in an Audio Units
// folder; a test should not install into the user's. Registered here, they
// are listed and opened in this process exactly as installed ones would be -
// from their bundles, by what their Info.plist says.
//==============================================================================

namespace
{

OSType fourCharCode (CFDictionaryRef component, CFStringRef key)
{
    const auto text = juce::String::fromCFString (static_cast<CFStringRef> (CFDictionaryGetValue (component, key)));
    OSType code = 0;
    for (auto i = 0; i < 4; ++i)
        code = (code << 8) | static_cast<OSType> (static_cast<unsigned char> (text[i]));
    return code;
}

/// Registers what the bundle's Info.plist lists under AudioComponents. The bundle stays loaded.
int registerComponentsOf (const juce::File& bundleFile)
{
    auto path = bundleFile.getFullPathName().toCFString();
    auto url = CFURLCreateWithFileSystemPath (nullptr, path, kCFURLPOSIXPathStyle, true);
    CFRelease (path);
    auto bundle = CFBundleCreate (nullptr, url);
    CFRelease (url);
    if (bundle == nullptr)
        return 0;

    auto components = static_cast<CFArrayRef> (CFBundleGetValueForInfoDictionaryKey (bundle, CFSTR ("AudioComponents")));
    if (components == nullptr || CFGetTypeID (components) != CFArrayGetTypeID())
        return 0;

    auto registered = 0;
    for (CFIndex i = 0; i < CFArrayGetCount (components); ++i)
    {
        auto component = static_cast<CFDictionaryRef> (CFArrayGetValueAtIndex (components, i));

        AudioComponentDescription description {};
        description.componentType = fourCharCode (component, CFSTR ("type"));
        description.componentSubType = fourCharCode (component, CFSTR ("subtype"));
        description.componentManufacturer = fourCharCode (component, CFSTR ("manufacturer"));

        SInt32 version = 0;
        if (auto number = static_cast<CFNumberRef> (CFDictionaryGetValue (component, CFSTR ("version"))))
            CFNumberGetValue (number, kCFNumberSInt32Type, &version);

        auto factory = CFBundleGetFunctionPointerForName (bundle, static_cast<CFStringRef> (CFDictionaryGetValue (component, CFSTR ("factoryFunction"))));
        if (factory == nullptr)
            continue;

        if (AudioComponentRegister (&description,
                                    static_cast<CFStringRef> (CFDictionaryGetValue (component, CFSTR ("name"))),
                                    static_cast<UInt32> (version),
                                    reinterpret_cast<AudioComponentFactoryFunction> (factory)) != nullptr)
            ++registered;
    }
    return registered;
}

} // namespace

int main (int argc, char* argv[])
{
    juce::StringArray arguments;
    for (auto i = 1; i < argc; ++i)
        arguments.add (juce::String::fromUTF8 (argv[i]));

    const auto pipeName = arguments[arguments.indexOf ("--serve") + 1];
    if (! arguments.contains ("--serve") || pipeName.isEmpty())
    {
        std::cerr << "usage: " << argv[0] << " scan-plugin --serve <pipe>" << std::endl;
        return 2;
    }

    const auto folder = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("TestAudioUnits");
    auto registered = 0;
    for (const auto& bundle : folder.findChildFiles (juce::File::findDirectories, false, "*.component"))
        registered += registerComponentsOf (bundle);
    if (registered == 0)
    {
        std::cerr << "no test Audio Units in " << folder.getFullPathName() << std::endl;
        return 1;
    }

    return audium::ScanWorker::serve (pipeName);
}
