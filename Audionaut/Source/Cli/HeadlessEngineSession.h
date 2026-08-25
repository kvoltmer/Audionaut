//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

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
 * Mirrors the lifecycle the Catch2 tests use: a MessageManager plus lock and
 * a factory-built engine, but never AudiumEngine::initialise() - that is the
 * one call that opens an audio device. Teardown order matters: the engine
 * must be released before DeletedAtShutdown::deleteAll() and
 * MessageManager::deleteInstance(), or Debug builds trip the leak detector.
 */
class HeadlessEngineSession {
public:
    HeadlessEngineSession()
    {
        juce::MessageManager::getInstance();
        messageManagerLock = std::make_unique<juce::MessageManagerLock> (juce::Thread::getCurrentThread());
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
        juce::DeletedAtShutdown::deleteAll();
        juce::MessageManager::deleteInstance();
    }

    AudiumEngine& operator*() const { return *engine; }
    AudiumEngine* operator->() const { return engine.get(); }
    std::shared_ptr<AudiumEngine> get() const { return engine; }

private:
    std::unique_ptr<juce::MessageManagerLock> messageManagerLock;
    std::shared_ptr<AudiumEngine> engine;

    JUCE_DECLARE_NON_COPYABLE (HeadlessEngineSession)
};

} // namespace cli
} // namespace audium
