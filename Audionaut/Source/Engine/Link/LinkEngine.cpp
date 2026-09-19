//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "LinkEngine.hpp"

// Make sure to define this before <cmath> is included for Windows
#ifdef LINK_PLATFORM_WINDOWS
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <iostream>

using namespace::ableton;
using namespace::std::chrono;

namespace audium
{

LinkEngine::LinkEngine() :
    mSampleRate(44100.),
    mOutputLatency(std::chrono::microseconds{0}),
    mSharedEngineData({0., false, false, 4., false}),
    mLockfreeEngineData(mSharedEngineData),
    mIsPlaying(false),
    mLink(std::make_unique<ableton::Link>(100.)),
    sessionState(mLink->captureAudioSessionState())
{
    if (!mOutputLatency.is_lock_free())
    {
        std::cout << "WARNING: LinkEngine::mOutputLatency is not lock free!" << std::endl;
    }
}

void LinkEngine::startPlaying()
{
    std::lock_guard<std::mutex> lock(mEngineDataGuard);
    mSharedEngineData.requestStart = true;
}

void LinkEngine::stopPlaying()
{
    std::lock_guard<std::mutex> lock(mEngineDataGuard);
    mSharedEngineData.requestStop = true;
    

}

bool LinkEngine::isPlaying() const
{
    return mLink->captureAppSessionState().isPlaying();
}

void LinkEngine::setTempo(double tempo)
{
    std::lock_guard<std::mutex> lock(mEngineDataGuard);
    mSharedEngineData.requestedTempo = tempo;
}

double LinkEngine::quantum() const
{
    return mSharedEngineData.quantum;
}

void LinkEngine::setQuantum(double quantum)
{
    std::lock_guard<std::mutex> lock(mEngineDataGuard);
    mSharedEngineData.quantum = quantum;
}

void LinkEngine::setStartPlayingTime(double beats)
{
    std::lock_guard<std::mutex> lock(mEngineDataGuard);
    mSharedEngineData.beatAtStartPlayingTime = beats;
}

bool LinkEngine::isStartStopSyncEnabled() const
{
    return mLink->isStartStopSyncEnabled();
}

void LinkEngine::setStartStopSyncEnabled(const bool enabled)
{
    mLink->enableStartStopSync(enabled);
}

void LinkEngine::setSampleRate(double sampleRate)
{
    mSampleRate = sampleRate;
}

void LinkEngine::enableLink(bool enabled)
{
    mLink->enable(enabled);
}

bool LinkEngine::isEnabled() const
{
    return mLink->isEnabled();
}

int LinkEngine::numPeers() const
{
    return static_cast<int>(mLink->numPeers());
}

LinkEngine::EngineData LinkEngine::pullEngineData()
{
    auto engineData = EngineData{};
    if (mEngineDataGuard.try_lock())
    {
        engineData.requestedTempo = mSharedEngineData.requestedTempo;
        mSharedEngineData.requestedTempo = 0;
        engineData.requestStart = mSharedEngineData.requestStart;
        mSharedEngineData.requestStart = false;
        engineData.requestStop = mSharedEngineData.requestStop;
        mSharedEngineData.requestStop = false;
        engineData.beatAtStartPlayingTime = mSharedEngineData.beatAtStartPlayingTime;
        mSharedEngineData.beatAtStartPlayingTime = -1.0;
        
        mLockfreeEngineData.quantum = mSharedEngineData.quantum;
        mLockfreeEngineData.startStopSyncOn = mSharedEngineData.startStopSyncOn;
        

        mEngineDataGuard.unlock();
    }
    engineData.quantum = mLockfreeEngineData.quantum;

    return engineData;
}

double LinkEngine::beatAtTime(std::chrono::microseconds time, double quantum) const
{
    return sessionState.beatAtTime(time, quantum);
}

bool LinkEngine::audioCallback(const std::chrono::microseconds hostTime,
                                const std::size_t numSamples)
{
    const auto engineData = pullEngineData();

    sessionState = mLink->captureAudioSessionState();

    if (engineData.requestStart)
    {
        sessionState.setIsPlaying(true, hostTime);
    }

    if (engineData.requestStop)
    {
        sessionState.setIsPlaying(false, hostTime);
    }

    if (!mIsPlaying && sessionState.isPlaying())
    {
        // Reset the timeline so that beat 0 corresponds to the time when transport starts
        sessionState.requestBeatAtStartPlayingTime(engineData.beatAtStartPlayingTime, engineData.quantum);
        mIsPlaying = true;
    }
    else if (mIsPlaying && !sessionState.isPlaying())
    {
        mIsPlaying = false;
    }

    if (engineData.requestedTempo > 0)
    {
        // Set the newly requested tempo from the beginning of this buffer
        sessionState.setTempo(engineData.requestedTempo, hostTime);
    }

    // Timeline modifications are complete, commit the results
    mLink->commitAudioSessionState(sessionState);

    return mIsPlaying;
}

} // namespace audium
