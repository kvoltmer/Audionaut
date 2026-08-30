//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

namespace audium {

/**
    Provides access to the application's persistent key-value settings.

    `Preferences` wraps the platform-specific storage backend behind a small
    interface for initialization, reading and writing string values, checking
    for existing keys, removing entries, and synchronizing pending changes.
*/
class Preferences
{
public:
    Preferences();
    ~Preferences();
    
    void init(const std::string& product, const std::string& manufacture);

    void synchronize();

    std::string getValue(const std::string& key,
                         const std::string& defaultValue = "");

    bool setValue(const std::string& key,
                  const std::string& value);

    bool valueExists(const std::string& key);

    void removeKey(const std::string& key);

private:
    
    class Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Preferences)
    
};

namespace PreferenceKeys
{
    static const char* const defaultFile            = "DefaultProjectFile";
    static const char* const lastProjectFile        = "LastProjectFile";
    static const char* const openLastProjectOnLaunch = "OpenLastProjectOnLaunch";
    static const char* const initialOpenDirectory   = "InitialOpenDirectory";
    static const char* const initialSaveDirectory   = "InitialSaveDirectory";
    static const char* const recentFiles            = "RecentProjectFiles";
    static const char* const audioDeviceSettings    = "AudioDeviceSettings";
    static const char* const mainWindowState        = "MainWindowState";
    static const char* const browserWindowState     = "BrowserWindowState";
    static const char* const autoEditWindowState    = "AutoEditWindowState";
    static const char* const browserWindowOpen      = "BrowserWindowOpen";
    static const char* const fileTreeState          = "FileTreeState";
    static const char* const browserRoot            = "BrowserRoot";
    static const char* const assembleMinutes        = "AssembleMinutes";
    static const char* const assembleSeconds        = "AssembleSeconds";
    static const char* const autoAnalysisEnabled    = "AutoAnalysisEnabled";
    static const char* const autoAnalysisTypes      = "AutoAnalysisTypes";
    static const char* const autoEditCrossfadesEnabled = "AutoEditCrossfadesEnabled";
    static const char* const autoEditCrossfadeMs    = "AutoEditCrossfadeMs";
    static const char* const autoEditCrossfadeType  = "AutoEditCrossfadeType";
    static const char* const usageStatsEnabled      = "UsageStatsEnabled";
    static const char* const analyticsClientId      = "AnalyticsClientId";
    static const char* const analyticsUnsentEvents  = "AnalyticsUnsentEvents";
}

} // namespace audium
