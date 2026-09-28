//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

// Make sure to define this before <cmath> is included for Windows
#define _USE_MATH_DEFINES

#include <JuceHeader.h>
#include <cstdint>

#if JUCE_WINDOWS
namespace audium::detail
{
constexpr std::uint64_t linkWindowsSwap64(const std::uint64_t value) noexcept
{
    return ((value & 0x00000000000000FFull) << 56)
         | ((value & 0x000000000000FF00ull) << 40)
         | ((value & 0x0000000000FF0000ull) << 24)
         | ((value & 0x00000000FF000000ull) << 8)
         | ((value & 0x000000FF00000000ull) >> 8)
         | ((value & 0x0000FF0000000000ull) >> 24)
         | ((value & 0x00FF000000000000ull) >> 40)
         | ((value & 0xFF00000000000000ull) >> 56);
}
}

#ifndef htonll
 #if JUCE_LITTLE_ENDIAN
  #define htonll(x) audium::detail::linkWindowsSwap64(x)
 #else
  #define htonll(x) (x)
 #endif
#endif

#ifndef ntohll
 #if JUCE_LITTLE_ENDIAN
  #define ntohll(x) audium::detail::linkWindowsSwap64(x)
 #else
  #define ntohll(x) (x)
 #endif
#endif
#endif

#if JUCE_MAC
    #define LINK_PLATFORM_MACOSX 1
#elif JUCE_WINDOWS
    #define LINK_PLATFORM_WINDOWS 1
#elif JUCE_LINUX
    #define LINK_PLATFORM_LINUX 1
#else
    #error "define the LINK_PLATFORM for this platform"
#endif


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

    void setSampleRate(double sampleRate);
    EngineData pullEngineData();
    
    double beatAtTime(std::chrono::microseconds time,
                      double quantum) const;
    
    bool audioCallback(const std::chrono::microseconds hostTime, std::size_t numSamples);

    
    double mSampleRate;
    std::atomic<std::chrono::microseconds> mOutputLatency;
    EngineData mSharedEngineData;
    EngineData mLockfreeEngineData;
    bool mIsPlaying;
    std::mutex mEngineDataGuard;

    static constexpr double beat_length = 1.;
    
private:
    std::unique_ptr<ableton::Link> mLink;   // before sessionState: it seeds it

    /// Re-captured at the start of every audio callback; a plain value
    /// (seeded in the constructor), since a fresh heap allocation per
    /// callback is not real-time safe. Audio thread only.
    ableton::Link::SessionState sessionState;
  
};


} // namespace audium
