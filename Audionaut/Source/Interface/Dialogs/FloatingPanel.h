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
 * owned; the panel takes its size and follows when the content changes it.
 * A panel made resizable lets the user drag its edges within the limits set,
 * and the content then takes the panel's size.
 */
class FloatingPanel final : public juce::Component
{
public:
    /// shortcuts, optional: the app's key mappings, so its shortcuts work with the panel in front.
    FloatingPanel (const juce::String& title, juce::Component* content, std::unique_ptr<juce::Component>& ownerPointer,
                   juce::KeyListener* shortcuts = nullptr, bool userResizable = false);
    ~FloatingPanel() override;

    juce::Component* getContentComponent() const noexcept { return content.get(); }

    /// For a resizable panel: how far the user may drag it (the content's size, without the title bar).
    void setResizeLimits (int minimumWidth, int minimumHeight, int maximumWidth, int maximumHeight);

    /// Brings the panel to the front and gives it the keyboard.
    void bringToFront();

    bool keyPressed (const juce::KeyPress& key) override;
    void childBoundsChanged (juce::Component* child) override;
    void resized() override;

private:
    struct Native;

    /// Clears the owner's pointer, asynchronously: the request comes from inside the native window's callbacks.
    void requestClose();

    /// The native window takes the panel's size (where the panel does not sit inside a window that follows it by itself).
    void applySizeToNative();

    std::unique_ptr<juce::Component>& owner;
    std::unique_ptr<juce::Component> content;
    std::unique_ptr<Native> native;
    const bool userResizable;
    bool settingSize = false;     ///< sizing the content to the panel: its change is not to come back as a panel size
    [[maybe_unused]] bool nativeResizing = false;  ///< the native window is being dragged: the panel follows it, not the other way round

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FloatingPanel)
};
