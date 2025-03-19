//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

//==============================================================================
class AudiumMenuModel  : public juce::MenuBarModel
{
public:
    AudiumMenuModel();

    juce::StringArray getMenuBarNames() override;

    juce::PopupMenu getMenuForIndex (int /*topLevelMenuIndex*/, const juce::String& menuName) override;

    void menuItemSelected (int menuItemID, int /*topLevelMenuIndex*/) override;
    
};
