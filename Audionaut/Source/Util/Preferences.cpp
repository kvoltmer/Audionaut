//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>

#if JUCE_WINDOWS
    #include "Util/native/PreferencesWindows.cpp"
#elif JUCE_MAC
    #include "Util/native/PreferencesMacOS.cpp"
#elif JUCE_LINUX
    #include "Util/native/PreferencesLinux.cpp"
#else
    #error "unsupported platform"
#endif
