/*
  ==============================================================================

    MainMenuModel.h
    Created: 11 May 2023 4:40:55pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

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
