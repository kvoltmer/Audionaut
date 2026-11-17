//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#import <AudioUnit/AUCocoaUIView.h>
#import <Cocoa/Cocoa.h>

#include <cstring>

#include <AudioUnitSDK/AUEffectBase.h>

//==============================================================================
// "Audionaut Tests: Crash On Editor" - an Audio Unit effect that is in order
// until its editor is opened. It scans, loads and passes its input through
// unchanged; it names a Cocoa view for its editor, and making that view
// takes the process down.
//==============================================================================

/// The editor's factory, which the host asks for the view.
@interface AudionautTestCrashOnEditorView : NSObject <AUCocoaUIBase>
@end

@implementation AudionautTestCrashOnEditorView

- (unsigned) interfaceVersion
{
    return 0;
}

- (NSView*) uiViewForAudioUnit: (AudioUnit) audioUnit withSize: (NSSize) preferredSize
{
    (void) audioUnit;
    (void) preferredSize;

    volatile int* nowhere = nullptr;
    *nowhere = 1;   // SIGSEGV: no exception a host could catch
    return nil;
}

@end

namespace
{

class CrashOnEditor final : public ausdk::AUEffectBase
{
public:
    explicit CrashOnEditor (AudioComponentInstance instance) :
        ausdk::AUEffectBase (instance)
    {
    }

    OSStatus GetPropertyInfo (AudioUnitPropertyID property, AudioUnitScope scope, AudioUnitElement element,
                              UInt32& outDataSize, bool& outWritable) override
    {
        if (property == kAudioUnitProperty_CocoaUI && scope == kAudioUnitScope_Global)
        {
            outDataSize = sizeof (AudioUnitCocoaViewInfo);
            outWritable = false;
            return noErr;
        }
        return ausdk::AUEffectBase::GetPropertyInfo (property, scope, element, outDataSize, outWritable);
    }

    OSStatus GetProperty (AudioUnitPropertyID property, AudioUnitScope scope, AudioUnitElement element, void* outData) override
    {
        if (property == kAudioUnitProperty_CocoaUI && scope == kAudioUnitScope_Global)
        {
            // where the view's class lives, and its name; both are the host's to release
            auto* bundle = [NSBundle bundleForClass: [AudionautTestCrashOnEditorView class]];
            AudioUnitCocoaViewInfo info;
            info.mCocoaAUViewBundleLocation = static_cast<CFURLRef> (CFBridgingRetain ([bundle bundleURL]));
            info.mCocoaAUViewClass[0] = static_cast<CFStringRef> (CFBridgingRetain (NSStringFromClass ([AudionautTestCrashOnEditorView class])));
            std::memcpy (outData, &info, sizeof (info));
            return noErr;
        }
        return ausdk::AUEffectBase::GetProperty (property, scope, element, outData);
    }

    OSStatus ProcessBufferLists (AudioUnitRenderActionFlags&, const AudioBufferList& input, AudioBufferList& output, UInt32) override
    {
        for (UInt32 i = 0; i < output.mNumberBuffers && i < input.mNumberBuffers; ++i)
            if (output.mBuffers[i].mData != input.mBuffers[i].mData)   // processed in place otherwise
                std::memcpy (output.mBuffers[i].mData, input.mBuffers[i].mData,
                             std::min (output.mBuffers[i].mDataByteSize, input.mBuffers[i].mDataByteSize));
        return noErr;
    }
};

} // namespace

// exports CrashOnEditorFactory, the factory the Info.plist names
AUSDK_COMPONENT_ENTRY (ausdk::AUBaseFactory, CrashOnEditor)
