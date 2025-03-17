//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once
#include <JuceHeader.h>

#include "Engine/Link/LinkEngine.hpp"
#include "Engine/ActionMessages.h"

namespace audium {

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
        tempoBPM = std::max(30.0, newTempo);
        if (linkEngine != nullptr)
        {
            linkEngine->setTempo(tempoBPM);
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
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate_)
    {
        sampleRate = sampleRate_;
        bufferSize = samplesPerBlockExpected;
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
    
    static double clicksToClocks(double clicks)
    {
        return clicks * 6.0;
    }
    
    static double clocksToClicks(double clocks)
    {
        return clocks / 6.0;
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
    
    static juce::String secondsToFormattedString(double timeSec)
    {
        int h = timeSec / (60 * 60);
        timeSec -= h * (60 * 60);
        
        int m = timeSec / (60);
        timeSec -= m * (60);
        
        int s = timeSec;
        timeSec -= s;
        
        std::ostringstream s1;
        s1 << std::setw(2) << std::setfill('0') << s;
        std::string ss1 = s1.str();
        
        std::ostringstream m1;
        m1 << std::setw(2) << std::setfill('0') << m;
        std::string mm1 = m1.str();
        
        juce::String timeFormated = "Total Length: " + juce::String(h) + ":" + mm1 + ":" + ss1;
        return timeFormated;
    }
    
    
private:
    std::shared_ptr<audium::LinkEngine> linkEngine;
    
    double tempoBPM = 120.0;
    
    double sampleRate = 0.0;
    
    int bufferSize = 0;
    
};

} // namespace audium

