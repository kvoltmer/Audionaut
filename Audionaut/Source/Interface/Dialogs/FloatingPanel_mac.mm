//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#import <Cocoa/Cocoa.h>

#include "FloatingPanel.h"

/// Hands the panel's close button to FloatingPanel, which closes the panel itself.
@interface AudionautFloatingPanelDelegate : NSObject<NSWindowDelegate>
{
@public
    std::function<void()> onClose;
    std::function<void()> onResize;
}
@end

@implementation AudionautFloatingPanelDelegate
- (BOOL) windowShouldClose: (NSWindow*) sender
{
    juce::ignoreUnused (sender);
    if (onClose != nullptr)
        onClose();
    return NO;
}

- (void) windowDidResize: (NSNotification*) notification
{
    juce::ignoreUnused (notification);
    if (onResize != nullptr)
        onResize();
}
@end

//==============================================================================
struct FloatingPanel::Native
{
    NSPanel* panel = nil;
    AudionautFloatingPanelDelegate* delegate = nil;
};

FloatingPanel::FloatingPanel (const juce::String& title, juce::Component* content_, std::unique_ptr<juce::Component>& ownerPointer,
                              juce::KeyListener* shortcuts, bool userResizable_) :
    owner (ownerPointer),
    content (content_),
    native (std::make_unique<Native>()),
    userResizable (userResizable_)
{
    addAndMakeVisible (content.get());
    const auto width = content->getWidth();
    const auto height = content->getHeight();
    if (shortcuts != nullptr)
        addKeyListener (shortcuts);   // the panel is the top of the key chain inside the native window

    JUCE_AUTORELEASEPOOL
    {
        NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskUtilityWindow;
        if (userResizable)
            style |= NSWindowStyleMaskResizable;
        native->panel = [[NSPanel alloc] initWithContentRect: NSMakeRect (0, 0, width, height)
                                                   styleMask: style
                                                     backing: NSBackingStoreBuffered
                                                       defer: NO];
        [native->panel setFloatingPanel: YES];       // above the app's windows
        [native->panel setHidesOnDeactivate: YES];   // and only while the app is in front
        [native->panel setReleasedWhenClosed: NO];
        [native->panel setAppearance: [NSAppearance appearanceNamed: NSAppearanceNameDarkAqua]];   // the app is dark whatever the system's mode
        [native->panel setTitle: [NSString stringWithUTF8String: title.toRawUTF8()]];

        native->delegate = [[AudionautFloatingPanelDelegate alloc] init];
        native->delegate->onClose = [this] { requestClose(); };
        native->delegate->onResize = [this]
        {
            // the user drags the panel's edge: the panel, and with it the content, takes the new size
            const auto size = [[native->panel contentView] frame].size;
            juce::ScopedValueSetter<bool> guard (nativeResizing, true);
            setSize (static_cast<int> (size.width), static_cast<int> (size.height));
        };
        [native->panel setDelegate: native->delegate];

        addToDesktop (0, [native->panel contentView]);
        setBounds (0, 0, width, height);
        setVisible (true);

        // centred over the window in front, JUCE's top-down screen points turned into Cocoa's bottom-up ones
        auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
        if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
            area = focused->getTopLevelComponent()->getScreenBounds();
        const auto frame = [native->panel frame];
        const auto primaryHeight = [[[NSScreen screens] objectAtIndex: 0] frame].size.height;
        const auto left = area.getCentreX() - frame.size.width / 2;
        const auto top = area.getCentreY() - frame.size.height / 2;
        [native->panel setFrameTopLeftPoint: NSMakePoint (left, primaryHeight - top)];

        [native->panel makeKeyAndOrderFront: nil];
    }

    owner.reset (this);
}

FloatingPanel::~FloatingPanel()
{
    removeFromDesktop();

    JUCE_AUTORELEASEPOOL
    {
        [native->panel setDelegate: nil];
        [native->panel close];
        [native->panel release];
        [native->delegate release];
    }
}

void FloatingPanel::bringToFront()
{
    [native->panel makeKeyAndOrderFront: nil];
    grabKeyboardFocus();
}

void FloatingPanel::setResizeLimits (int minimumWidth, int minimumHeight, int maximumWidth, int maximumHeight)
{
    if (native->panel == nil)
        return;
    [native->panel setContentMinSize: NSMakeSize (minimumWidth, minimumHeight)];
    [native->panel setContentMaxSize: NSMakeSize (maximumWidth, maximumHeight)];
}

void FloatingPanel::applySizeToNative()
{
    // the content grew or shrank (a plugin resized its editor): the panel's window takes the size, its top left staying put
    if (native == nullptr || native->panel == nil || nativeResizing)
        return;

    const auto frame = [native->panel frame];
    const auto current = [native->panel contentRectForFrameRect: frame].size;
    if (static_cast<int> (current.width) == getWidth() && static_cast<int> (current.height) == getHeight())
        return;

    [native->panel setContentSize: NSMakeSize (getWidth(), getHeight())];
    [native->panel setFrameTopLeftPoint: NSMakePoint (frame.origin.x, NSMaxY (frame))];
}
