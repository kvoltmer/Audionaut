/*
  ==============================================================================

    TempoProvider.h
    Created: 14 Nov 2023 4:09:49pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

#include "Engine/Link/LinkEngine.hpp"
#include "Engine/ActionMessages.h"

// Provide and store the current tempo
class TempoProvider : public juce::ActionBroadcaster
{
public:
    
    TempoProvider(std::shared_ptr<audium::LinkEngine> linkEngine) :
        linkEngine(linkEngine)
    {
        linkEngine->getLink()->setTempoCallback([this](const double p) { onTempoChange(p); });
        linkEngine->getLink()->enable(false);
        linkEngine->setTempo(tempoBPM);
    }
    
    void setTempo(double newTempo)
    {
        tempoBPM = newTempo;
        if (linkEngine != nullptr)
        {
            linkEngine->setTempo(newTempo);
        }
        
        sendActionMessage (tempoChanged);
    }
    
    double getTempo() const
    {
        return tempoBPM;
    }
    
    void onTempoChange(double newTempo)
    {
        tempoBPM = newTempo;
        sendActionMessage (tempoChanged);
    }
    
    void prepareToPlay (double newSampleRate, int newBlockSize)
    {
        sampleRate = newSampleRate;
        bufferSize = newBlockSize;
    }
    
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
    
    double secondsToClocks(double seconds) const
    {
        return secondsToClocks(getTempo(), seconds);
    }

    double clocksToSeconds(double clocks) const
    {
        return clocksToSeconds(getTempo(), clocks);
    }
    
    juce::Range<double> secondsToClocks(juce::Range<double> rangeInSeconds)
    {
        const auto start = secondsToClocks(rangeInSeconds.getStart());
        const auto end = secondsToClocks(rangeInSeconds.getEnd());
        return juce::Range<double>(start, end);
    }
    
    juce::Range<double> clocksToSeconds(juce::Range<double> rangeInClocks)
    {
        const auto start = clocksToSeconds(rangeInClocks.getStart());
        const auto end = clocksToSeconds(rangeInClocks.getEnd());
        return juce::Range<double>(start, end);
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
    
    static double barsToClocks(double bars)
    {
        return bars * 96.0;
    }
    
    static double clocksToBars(double clocks)
    {
        return clocks / 96.0;
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
    
    double secondsToBeats(double seconds)
    {
        return secondsToBeats(getTempo(), seconds);
    }

    double beatsToSeconds(double beats)
    {
        return beatsToSeconds(getTempo(), beats);
    }

    
private:
    std::shared_ptr<audium::LinkEngine> linkEngine;
    
    double tempoBPM = 120.0;
    
    double sampleRate = 0.0;
    
    int bufferSize = 0;
    
};
