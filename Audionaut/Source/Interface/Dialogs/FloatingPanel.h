//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

/**
 * A palette window in the platform's own utility style, for small tools that
 * stay beside the main window (the effect editors): the slim native title
 * bar with a close button only, floating above the app's other windows and
 * out of sight while another app is in front.
 *
 * macOS: an NSPanel with the utility window style, the panel attached to its
 * content view (JUCE has no panel windows of its own). Elsewhere: a native
 * title bar window kept off the taskbar (a tool window with the slim caption
 * on Windows), on top while the app is in front.
 *
 * Like FloatingToolWindow it assigns itself into the caller's owning pointer
 * and clears it again on close (the close button or Escape). The content is
 * owned and keeps its size.
 */
class FloatingPanel final : public juce::Component
{
public:
    /// shortcuts, optional: the app's key mappings, so its shortcuts work with the panel in front.
    FloatingPanel (const juce::String& title, juce::Component* content, std::unique_ptr<juce::Component>& ownerPointer,
                   juce::KeyListener* shortcuts = nullptr);
    ~FloatingPanel() override;

    juce::Component* getContentComponent() const noexcept { return content.get(); }

    /// Brings the panel to the front and gives it the keyboard.
    void bringToFront();

    bool keyPressed (const juce::KeyPress& key) override;

private:
    struct Native;

    /// Clears the owner's pointer, asynchronously: the request comes from inside the native window's callbacks.
    void requestClose();

    std::unique_ptr<juce::Component>& owner;
    std::unique_ptr<juce::Component> content;
    std::unique_ptr<Native> native;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FloatingPanel)
};
