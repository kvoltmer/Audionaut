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
@end

//==============================================================================
struct FloatingPanel::Native
{
    NSPanel* panel = nil;
    AudionautFloatingPanelDelegate* delegate = nil;
};

FloatingPanel::FloatingPanel (const juce::String& title, juce::Component* content_, std::unique_ptr<juce::Component>& ownerPointer,
                              juce::KeyListener* shortcuts) :
    owner (ownerPointer),
    content (content_),
    native (std::make_unique<Native>())
{
    addAndMakeVisible (content.get());
    const auto width = content->getWidth();
    const auto height = content->getHeight();
    if (shortcuts != nullptr)
        addKeyListener (shortcuts);   // the panel is the top of the key chain inside the native window

    JUCE_AUTORELEASEPOOL
    {
        native->panel = [[NSPanel alloc] initWithContentRect: NSMakeRect (0, 0, width, height)
                                                   styleMask: NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskUtilityWindow
                                                     backing: NSBackingStoreBuffered
                                                       defer: NO];
        [native->panel setFloatingPanel: YES];       // above the app's windows
        [native->panel setHidesOnDeactivate: YES];   // and only while the app is in front
        [native->panel setReleasedWhenClosed: NO];
        [native->panel setAppearance: [NSAppearance appearanceNamed: NSAppearanceNameDarkAqua]];   // the app is dark whatever the system's mode
        [native->panel setTitle: [NSString stringWithUTF8String: title.toRawUTF8()]];

        native->delegate = [[AudionautFloatingPanelDelegate alloc] init];
        native->delegate->onClose = [this] { requestClose(); };
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
