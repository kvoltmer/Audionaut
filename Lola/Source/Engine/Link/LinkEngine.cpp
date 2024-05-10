
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
    mTimeAtLastClick{},
    mIsPlaying(false)
{
    mLink.reset(new ableton::Link(100.f));
        
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

double LinkEngine::beatTime() const
{
    return sessionState->beatAtTime(mLink->clock().micros(), mSharedEngineData.quantum);
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

//double LinkEngine::getStartPlayingTime() const
//{
//    return mSharedEngineData.beatAtStartPlayingTime;
//}
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

void LinkEngine::setBufferSize(std::size_t size)
{
    mBuffer = std::vector<double>(size, 0.);
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

void LinkEngine::renderMetronomeIntoBuffer( const double quantum,
                                            const std::chrono::microseconds beginHostTime,
                                            const std::size_t numSamples)
{

    // Metronome frequencies
    static const double highTone = 1567.98;
    static const double lowTone = 1108.73;
    // 100ms click duration
    static const auto clickDuration = duration<double>{0.1};

    // The number of microseconds that elapse between samples
    const auto microsPerSample = 1e6 / mSampleRate;

    for (std::size_t i = 0; i < numSamples; ++i)
    {
        double amplitude = 0.;
        // Compute the host time for this sample and the last.
        const auto hostTime = beginHostTime + microseconds(llround(static_cast<double>(i) * microsPerSample));
        const auto lastSampleHostTime = hostTime - microseconds(llround(microsPerSample));

        // Only make sound for positive beat magnitudes. Negative beat
        // magnitudes are count-in beats.
        if (sessionState->beatAtTime(hostTime, quantum) >= 0.)
        {
            // If the phase wraps around between the last sample and the
            // current one with respect to a 1 beat quantum, then a click
            // should occur.
            if (sessionState->phaseAtTime(hostTime, 1)
              < sessionState->phaseAtTime(lastSampleHostTime, 1))
            {
                mTimeAtLastClick = hostTime;
            }

            const auto secondsAfterClick =
            duration_cast<duration<double>>(hostTime - mTimeAtLastClick);

            // If we're within the click duration of the last beat, render
            // the click tone into this sample
            if (secondsAfterClick < clickDuration)
            {
                // If the phase of the last beat with respect to the current
                // quantum was zero, then it was at a quantum boundary and we
                // want to use the high tone. For other beats within the
                // quantum, use the low tone.
                const auto freq =
                floor(sessionState->phaseAtTime(hostTime, quantum)) == 0 ? highTone : lowTone;

                // Simple cosine synth
                amplitude = cos(2 * M_PI * secondsAfterClick.count() * freq)
                        * (1 - sin(5 * M_PI * secondsAfterClick.count()));
            }
        }
        mBuffer[i] = amplitude;
    }
}

void LinkEngine::triggerScheduler(const double quantum,
                                  const std::chrono::microseconds beginHostTime,
                                  const std::size_t numSamples)
{
    const auto beats = sessionState->beatAtTime(beginHostTime, quantum);
    
    // Call the PlayListScheduler
    tickCallback(sessionState->isPlaying(), beats, static_cast<int>(numSamples));
    
    
    // The number of microseconds that elapse between samples
    const auto microsPerSample = 1e6 / mSampleRate;
    for (std::size_t i = 0; i < numSamples; ++i)
    {
        // Compute the host time for this sample and the last.
        const auto hostTime = beginHostTime + microseconds(llround(static_cast<double>(i) * microsPerSample));
        const auto lastSampleHostTime = hostTime - microseconds(llround(microsPerSample));
        
        // Only make sound for positive beat magnitudes. Negative beat
        // magnitudes are count-in beats.
        auto beats = sessionState->beatAtTime(hostTime, quantum);
        if (beats >= 0.)
        {
            
            // If the phase wraps around between the last sample and the
            // current one with respect to a 1 beat quantum, then a sample trigger
            // should occur.
            if (sessionState->phaseAtTime(hostTime, beat_length)
                < sessionState->phaseAtTime(lastSampleHostTime, beat_length))
            {
                seconds s = std::chrono::duration_cast<seconds>(hostTime);
                std::cout << "onTriggerBeat " << beats << " " << s.count() << " " << i << std::endl;
            }
        }
    }
    
    
}

void LinkEngine::audioCallback(const std::chrono::microseconds hostTime,
                                const std::size_t numSamples)
{
    const auto engineData = pullEngineData();

    sessionState = std::make_unique<ableton::Link::SessionState>(mLink->captureAudioSessionState());

    // Clear the buffer
    std::fill(mBuffer.begin(), mBuffer.end(), 0);

    if (engineData.requestStart)
    {
        sessionState->setIsPlaying(true, hostTime);
    }

    if (engineData.requestStop)
    {
        sessionState->setIsPlaying(false, hostTime);
    }

    if (!mIsPlaying && sessionState->isPlaying())
    {
        // Reset the timeline so that beat 0 corresponds to the time when transport starts
        sessionState->requestBeatAtStartPlayingTime(engineData.beatAtStartPlayingTime, engineData.quantum);
        mIsPlaying = true;
    }
    else if (mIsPlaying && !sessionState->isPlaying())
    {
        mIsPlaying = false;
    }

    if (engineData.requestedTempo > 0)
    {
        // Set the newly requested tempo from the beginning of this buffer
        sessionState->setTempo(engineData.requestedTempo, hostTime);
    }

    // Timeline modifications are complete, commit the results
    mLink->commitAudioSessionState(*sessionState);

    if (mIsPlaying)
    {
        // As long as the engine is playing, generate metronome clicks in
        // the buffer at the appropriate beats.
        // renderMetronomeIntoBuffer(engineData.quantum, hostTime, numSamples);
        
        triggerScheduler(engineData.quantum, hostTime, numSamples);
    }
}

} // namespace audium
