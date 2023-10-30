/*
  ==============================================================================

    PlayListScheduler.h
    Created: 5 Jul 2023 3:22:44pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "Engine/AudioRegion.h"
#include "Engine/PlayList/SampleTimer.h"
#include "Engine/Link/LinkEngine.hpp"

class TransportSourceContainer;
class PlayListContainer;
class PlayListItem;
class AudioGroupContainer;


class PlayListScheduler
{
    
    
public:
    PlayListScheduler(std::shared_ptr<AudioGroupContainer> audioGroupContainer);
    ~PlayListScheduler();

    void prepareToPlay (double sampleRate, int blockSize);
    

    void startPlaying();
    void stopPlaying();
    bool isPlaying() const;
    
    void setPlayListItemIndex(int playListItemIndex);
    int getPlayListItemIndex(std::shared_ptr<AudioGroup> group) const;
    double getPlayListItemProgress(std::shared_ptr<AudioGroup> group, int playListItemIndex) const;
    
    double getAbsolutePositionClocks() const;
    
    double getAbsolutePosition() const;
    void setAbsolutePosition(double newPosition);
    
    void tick(ableton::Link::SessionState *sessionState,
              const double quantum,
              const std::chrono::microseconds beginHostTime,
              int numSamples);
    
    double getTotalLength() const;
    
    void onTriggerBeat(const double beatTime, const std::chrono::microseconds hostTime, int sampleNumber);
    
    void setLinkEngine(audium::LinkEngine* engine);
    audium::LinkEngine* getLinkEngine() const { return linkEngine; }
    
    double getTempo() const;
    void setTempo(double newTempo);
    
    uint64_t secondsToSamples(double seconds)
    {
        return static_cast<uint64_t>(seconds * sampleRate);
    }
    
    double samplesToSeconds(uint64_t samples)
    {
        jassert(sampleRate > 0.0);
        return static_cast<double>(samples) / sampleRate;
    }
    
    static double secondsToClocks(double tempo, double seconds)
    {
        return tempo * 0.4 * seconds;
    }

    static double clocksToSeconds(double tempo, double clocks)
    {
        jassert(tempo > 0.0);
        return clocks / (tempo * 0.4);
    }
    
    static double beatsToClocks(double beats)
    {
        // 4th * 24 = 96th
        return beats * 24.0;
    }
    
    static double clocksToBeats(double clocks)
    {
        // 96th / 24 = 4th
        return clocks / 24.0;
    }
    
    static double secondsToBeats(double tempo, double seconds)
    {
        return clocksToBeats(secondsToClocks(tempo, seconds));
    }

    static double beatsToSeconds(double tempo, double beats)
    {
        jassert(tempo > 0.0);
        return clocksToSeconds(tempo, beatsToClocks(beats));
    }
    
private:

    double absoluteToLocalPosition(double absolutePosition, const PlayListItem* item) const;
    void applyAbsolutePosition(double pos);
    
    std::shared_ptr<AudioGroupContainer> audioGroupContainer;
    
    double sampleRate = 0.0;
    int bufferSize = 0;
    
    // transport position in 96th clocks
    double transportPositionClocks = 0.0;
    
    double startPositionClocks = 0.0;
    
    std::atomic<bool> forcePosition = false;
    
    juce::CriticalSection readLock;
    
    audium::LinkEngine *linkEngine;
        
    double tempoBPM = 120.0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListScheduler)
};
