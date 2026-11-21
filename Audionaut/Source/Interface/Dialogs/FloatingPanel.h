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

    /// Puts the window's top left corner at a place on the desktop (it opens centred over the window in front).
    void setTopLeft (juce::Point<int> topLeft);

    /// Told the window's top left corner on the desktop when the user moved the window - not when the panel
    /// was put somewhere (setTopLeft(), the place it opens at) or took a new size.
    std::function<void (juce::Point<int> topLeft)> onMoved;

    /// Told when the user closes the window (the close button or Escape), before it goes.
    std::function<void()> onUserClosed;

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

    /// The native window says where its top left corner is now; onMoved hears of it where that is a new
    /// place the panel did not go to by itself.
    void nativeMovedTo (juce::Point<int> topLeft);

    std::unique_ptr<juce::Component>& owner;
    std::unique_ptr<juce::Component> content;
    std::unique_ptr<Native> native;
    const bool userResizable;
    juce::Point<int> lastTopLeft;   ///< where the native window was last seen or put
    bool placing = false;         ///< the panel moves its native window itself: no move of the user's
    bool settingSize = false;     ///< sizing the content to the panel: its change is not to come back as a panel size
    [[maybe_unused]] bool nativeResizing = false;  ///< the native window is being dragged: the panel follows it, not the other way round

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FloatingPanel)
};
