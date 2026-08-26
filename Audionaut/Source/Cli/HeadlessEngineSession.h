//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <memory>
#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/Analysis/AnalysisWorker.h"
#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"

namespace audium {
namespace cli {

/**
 * @class HeadlessEngineSession
 * @brief RAII wrapper for a GUI-free engine lifetime.
 *
 * Two modes:
 *
 * Owning (default, console binary and tests): mirrors the lifecycle the
 * Catch2 tests use - creates the MessageManager plus lock and tears both
 * down afterwards (DeletedAtShutdown::deleteAll + deleteInstance).
 *
 * External (setUseExternalMessageManager(true), set by the GUI app before
 * an in-app CLI run): the process owns a running MessageManager and the
 * session runs on the message thread itself, so the session only owns the
 * engine - no lock, and crucially no MessageManager/DeletedAtShutdown
 * teardown, which would destroy the running application from inside
 * initialise(). DeletedAtShutdown objects the engine creates are cleaned
 * up once by JUCE's own shutdownApp at process exit.
 *
 * Either way the engine is built by the factory and
 * AudiumEngine::initialise() is never called - that is the one call that
 * opens an audio device. Owning-mode teardown order matters: the engine
 * must be released before DeletedAtShutdown::deleteAll() and
 * MessageManager::deleteInstance(), or Debug builds trip the leak detector.
 */
class HeadlessEngineSession {
public:
    /** GUI app only: make sessions borrow the app's MessageManager. */
    static void setUseExternalMessageManager (bool external) { externalFlag().store (external); }

    HeadlessEngineSession() :
        ownsMessageManager (! externalFlag().load())
    {
        if (ownsMessageManager) {
            juce::MessageManager::getInstance();
            messageManagerLock = std::make_unique<juce::MessageManagerLock> (juce::Thread::getCurrentThread());
        }
        engine = AudiumFactory::createAudiumEngine();
    }

    ~HeadlessEngineSession()
    {
        // A file import may have auto-enqueued background analysis; the worker
        // thread must drain before the object graph goes away underneath it.
        if (auto worker = engine->getAudioResourceContainer()->getAnalysisWorker())
            while (worker->isBusy())
                juce::Thread::sleep (50);

        messageManagerLock = nullptr;
        engine = nullptr;

        if (ownsMessageManager) {
            juce::DeletedAtShutdown::deleteAll();
            juce::MessageManager::deleteInstance();
        }
    }

    AudiumEngine& operator*() const { return *engine; }
    AudiumEngine* operator->() const { return engine.get(); }
    std::shared_ptr<AudiumEngine> get() const { return engine; }

private:
    static std::atomic<bool>& externalFlag()
    {
        static std::atomic<bool> value { false };
        return value;
    }

    const bool ownsMessageManager;
    std::unique_ptr<juce::MessageManagerLock> messageManagerLock;
    std::shared_ptr<AudiumEngine> engine;

    JUCE_DECLARE_NON_COPYABLE (HeadlessEngineSession)
};

} // namespace cli
} // namespace audium
