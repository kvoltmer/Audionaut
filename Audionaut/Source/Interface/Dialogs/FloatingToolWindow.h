
//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <memory>
#include <JuceHeader.h>

#include <Application/AudiumApplication.h>


class FloatingToolWindow final : public DialogWindow
{
public:
    FloatingToolWindow (const std::string& windowTitle,
                        const std::string& windowPositionSettingName_,
                        Component* content,
                        std::unique_ptr<Component>& ownerPointer,
                        bool shouldBeResizable,
                        int defaultW, int defaultH,
                        int minW, int minH,
                        int maxW, int maxH) :
        DialogWindow (windowTitle,
                      content->findColour (audium::secondaryBackgroundColourId), true, true),
                      windowPositionSettingName (windowPositionSettingName_),
        owner (ownerPointer)
    {
        setUsingNativeTitleBar (true);
        setResizable (shouldBeResizable, shouldBeResizable);
        setResizeLimits (minW, minH, maxW, maxH);
        setContentOwned (content, false);

        std::string windowState;
        if (not windowPositionSettingName.empty())
            windowState = AudiumApplication::getPreferences().getValue (windowPositionSettingName);
        
        if (not windowState.empty())
            restoreWindowStateFromString (windowState);
        else
            centreAroundComponent (Component::getCurrentlyFocusedComponent(), defaultW, defaultH);

        setVisible (true);
        owner.reset (this);
    }

    ~FloatingToolWindow() override
    {
        if (not windowPositionSettingName.empty())
            AudiumApplication::getPreferences().setValue (windowPositionSettingName,
                                                          getWindowStateAsString().toStdString());
            
  
    }

    void closeButtonPressed() override
    {
        owner.reset();
    }

    bool escapeKeyPressed() override
    {
        closeButtonPressed();
        return true;
    }

    void paint (Graphics& g) override
    {
        g.fillAll (findColour (audium::secondaryBackgroundColourId));
    }

private:
    std::string windowPositionSettingName;
    std::unique_ptr<Component>& owner;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FloatingToolWindow)
};
