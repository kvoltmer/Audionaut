
#pragma once

#if __APPLE__
    #define LINK_PLATFORM_MACOSX 1
#else
    #error "define the LINK_PLATFORM for this platform"
#endif

// Make sure to define this before <cmath> is included for Windows
#define _USE_MATH_DEFINES
#include <ableton/Link.hpp>
#include <atomic>
#include <mutex>



namespace audium
{

class LinkEngine
{
public:
    LinkEngine();
    
    ableton::Link* getLink() const { return mLink.get(); }

    void startPlaying();
    void stopPlaying();
    bool isPlaying() const;
    double beatTime() const;
    void setTempo(double tempo);
    double quantum() const;
    void setQuantum(double quantum);
    bool isStartStopSyncEnabled() const;
    void setStartStopSyncEnabled(bool enabled);
    void setStartPlayingTime(double beats);

    void enableLink(bool enabled);
    bool isEnabled() const;
    int numPeers() const;
public:
    struct EngineData
    {
        double requestedTempo;
        bool requestStart;
        bool requestStop;
        double quantum;
        bool startStopSyncOn;
        double beatAtStartPlayingTime = 0.0;
    };

    void setBufferSize(std::size_t size);
    void setSampleRate(double sampleRate);
    EngineData pullEngineData();
    void renderMetronomeIntoBuffer(double quantum,
                                   std::chrono::microseconds beginHostTime,
                                   std::size_t numSamples);

    void triggerScheduler(const double quantum,
                          const std::chrono::microseconds beginHostTime,
                          const std::size_t numSamples);
    
    void audioCallback(const std::chrono::microseconds hostTime, std::size_t numSamples);

    
    double mSampleRate;
    std::atomic<std::chrono::microseconds> mOutputLatency;
    std::vector<double> mBuffer;
    EngineData mSharedEngineData;
    EngineData mLockfreeEngineData;
    std::chrono::microseconds mTimeAtLastClick;
    bool mIsPlaying;
    std::mutex mEngineDataGuard;

    static constexpr double beat_length = 1.;

    std::function<void(bool, double, int)> tickCallback;
    
private:
    
    std::unique_ptr<ableton::Link::SessionState> sessionState;
    
    std::unique_ptr<ableton::Link> mLink;
  
};


} // namespace audium
