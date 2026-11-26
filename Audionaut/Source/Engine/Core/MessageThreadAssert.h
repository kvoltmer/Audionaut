//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

namespace audium
{

/**
 * Whether the calling thread may touch what belongs to the message thread:
 * it is the message thread, or it holds the message manager's lock (an
 * export thread does, for the steps it takes there). Where no message
 * manager exists - a test that runs the engine on its own one thread -
 * there is nothing to race with, and every thread may.
 */
inline bool mayTouchMessageThreadState() noexcept
{
    const auto* messageManager = juce::MessageManager::getInstanceWithoutCreating();
    return messageManager == nullptr || messageManager->currentThreadHasLockedMessageManager();
}

} // namespace audium

/**
 * At the entrance of code that only the message thread may run: stops a
 * debug build on the caller that came from another thread, where the damage
 * it does would show up later and elsewhere.
 */
#define AUDIUM_ASSERT_MESSAGE_THREAD jassert (audium::mayTouchMessageThreadState());
