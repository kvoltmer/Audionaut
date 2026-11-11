//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "FloatingPanel.h"

#include "Interface/ColourIds.h"

// the parts every platform shares; the native window lives in FloatingPanel_mac.mm on macOS, here elsewhere

bool FloatingPanel::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) {
        requestClose();
        return true;
    }
    return false;
}

void FloatingPanel::requestClose()
{
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<FloatingPanel> (this)]
    {
        if (safeThis != nullptr)
            safeThis->owner.reset();
    });
}

#if ! JUCE_MAC

//==============================================================================
struct FloatingPanel::Native final : public juce::DocumentWindow,
                                     private juce::Timer
{
    Native (FloatingPanel& panel_, const juce::String& title, juce::KeyListener* shortcuts) :
        juce::DocumentWindow (title, panel_.findColour (audium::secondaryBackgroundColourId), juce::DocumentWindow::closeButton, false),
        panel (panel_)
    {
        setUsingNativeTitleBar (true);
        setResizable (false, false);
        setContentNonOwned (&panel, true);
        setAlwaysOnTop (true);
        if (shortcuts != nullptr)
            addKeyListener (shortcuts);   // the window takes the keys while nothing inside has the focus
        centreAroundComponent (juce::Component::getCurrentlyFocusedComponent(), getWidth(), getHeight());
        addToDesktop();   // only now, the constructor would not see the style flags below
        setVisible (true);
        startTimerHz (4);
    }

    ~Native() override
    {
        clearContentComponent();
    }

    int getDesktopWindowStyleFlags() const override
    {
        // off the taskbar: on Windows that makes it a tool window, with the slim caption
        return juce::DocumentWindow::getDesktopWindowStyleFlags() & ~juce::ComponentPeer::windowAppearsOnTaskbar;
    }

    void closeButtonPressed() override
    {
        panel.requestClose();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        return panel.keyPressed (key) || juce::DocumentWindow::keyPressed (key);
    }

    void timerCallback() override
    {
        // on top of this app only: out of sight while another app is in front
        const auto foreground = juce::Process::isForegroundProcess();
        if (isVisible() != foreground)
            setVisible (foreground);
    }

    FloatingPanel& panel;
};

//==============================================================================
FloatingPanel::FloatingPanel (const juce::String& title, juce::Component* content_, std::unique_ptr<juce::Component>& ownerPointer,
                              juce::KeyListener* shortcuts) :
    owner (ownerPointer),
    content (content_)
{
    addAndMakeVisible (content.get());
    setSize (content->getWidth(), content->getHeight());
    native = std::make_unique<Native> (*this, title, shortcuts);
    owner.reset (this);
}

FloatingPanel::~FloatingPanel()
{
    native.reset();
}

void FloatingPanel::bringToFront()
{
    native->setVisible (true);
    native->toFront (true);
}

#endif
