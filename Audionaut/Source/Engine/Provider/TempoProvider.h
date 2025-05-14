//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <JuceHeader.h>

#include "Engine/Link/LinkEngine.hpp"
#include "Engine/ActionMessages.h"

namespace audium {

/**
 * @class TempoProvider
 * @brief Provides and manages the current tempo for the application.
 *
 * The `TempoProvider` class handles tempo-related operations, including conversions
 * between time units (seconds, beats, clocks, etc.) and broadcasting tempo changes.
 */
class TempoProvider : public juce::ActionBroadcaster
{
public:
    /**
     * @brief Constructs a `TempoProvider` with the specified `LinkEngine`.
     * @param linkEngine_ A shared pointer to the `LinkEngine`.
     */
    TempoProvider(std::shared_ptr<audium::LinkEngine> linkEngine_) :
        linkEngine(linkEngine_)
    {
        linkEngine->getLink()->setTempoCallback([this](const double p) { onTempoChange(p); });
        linkEngine->getLink()->enable(false);
        linkEngine->setTempo(tempoBPM);
    }
    
    /**
     * @brief Sets the tempo in beats per minute (BPM).
     * @param newTempo The new tempo value.
     */
    void setTempo(double newTempo)
    {
        tempoBPM = std::max(30.0, newTempo);
        if (linkEngine != nullptr)
        {
            linkEngine->setTempo(tempoBPM);
        }
        
        sendActionMessage (tempoChanged);
    }
    
    /**
     * @brief Retrieves the current tempo in beats per minute (BPM).
     * @return The current tempo.
     */
    double getTempo() const
    {
        return tempoBPM;
    }
    
    /**
     * @brief Handles changes to the tempo.
     * @param newTempo The new tempo value.
     */
    void onTempoChange(double newTempo)
    {
        tempoBPM = newTempo;
        sendActionMessage (tempoChanged);
    }
    
    /**
     * @brief Prepares the provider for playback.
     * @param samplesPerBlockExpected The expected number of samples per block.
     * @param sampleRate_ The sample rate in Hz.
     */
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate_)
    {
        sampleRate = sampleRate_;
        bufferSize = samplesPerBlockExpected;
    }
    
    /**
     * @brief Converts seconds to samples.
     * @param seconds The time in seconds.
     * @return The equivalent time in samples.
     */
    uint64_t secondsToSamples(double seconds)
    {
        return static_cast<uint64_t>(seconds * sampleRate);
    }
    
    /**
     * @brief Converts samples to seconds.
     * @param samples The time in samples.
     * @return The equivalent time in seconds.
     */
    double samplesToSeconds(uint64_t samples)
    {
        jassert(sampleRate > 0.0);
        return static_cast<double>(samples) / sampleRate;
    }
    
    /**
     * @brief Converts seconds to clocks based on the tempo.
     * @param seconds The time in seconds.
     * @return The equivalent time in clocks.
     */
    static double secondsToClocks(double tempo, double seconds)
    {
        return tempo * 0.4 * seconds;
    }
    
    /**
     * @brief Converts clocks to seconds based on the tempo.
     * @param clocks The time in clocks.
     * @return The equivalent time in seconds.
     */
    static double clocksToSeconds(double tempo, double clocks)
    {
        jassert(tempo > 0.0);
        return clocks / (tempo * 0.4);
    }
    
    /**
     * @brief Converts seconds to clocks based on the tempo.
     * @param seconds The time in seconds.
     * @return The equivalent time in clocks.
     */
    double secondsToClocks(double seconds) const
    {
        return secondsToClocks(getTempo(), seconds);
    }
    
    /**
     * @brief Converts clocks to seconds based on the tempo.
     * @param clocks The time in clocks.
     * @return The equivalent time in seconds.
     */
    double clocksToSeconds(double clocks) const
    {
        return clocksToSeconds(getTempo(), clocks);
    }
    
    /**
     * @brief Converts a range of seconds to clocks.
     * @param rangeInSeconds The range in seconds.
     * @return The equivalent range in clocks.
     */
    juce::Range<double> secondsToClocks(juce::Range<double> rangeInSeconds)
    {
        const auto start = secondsToClocks(rangeInSeconds.getStart());
        const auto end = secondsToClocks(rangeInSeconds.getEnd());
        return juce::Range<double>(start, end);
    }
    
    /**
     * @brief Converts a range of clocks to seconds.
     * @param rangeInClocks The range in clocks.
     * @return The equivalent range in seconds.
     */
    juce::Range<double> clocksToSeconds(juce::Range<double> rangeInClocks)
    {
        const auto start = clocksToSeconds(rangeInClocks.getStart());
        const auto end = clocksToSeconds(rangeInClocks.getEnd());
        return juce::Range<double>(start, end);
    }
    
    /**
     * @brief Converts beats to clocks.
     * @param beats The time in beats.
     * @return The equivalent time in clocks.
     */
    static double beatsToClocks(double beats)
    {
        // 4th * 24 = 96th
        return beats * 24.0;
    }
    
    /**
     * @brief Converts clocks to beats.
     * @param clocks The time in clocks.
     * @return The equivalent time in beats.
     */
    static double clocksToBeats(double clocks)
    {
        // 96th / 24 = 4th
        return clocks / 24.0;
    }
    
    /**
     * @brief Converts clicks to clocks.
     * @param clicks The time in clicks.
     * @return The equivalent time in clocks.
     */
    static double clicksToClocks(double clicks)
    {
        return clicks * 6.0;
    }
    
    /**
     * @brief Converts clocks to clicks.
     * @param clocks The time in clocks.
     * @return The equivalent time in clicks.
     */
    static double clocksToClicks(double clocks)
    {
        return clocks / 6.0;
    }
    
    /**
     * @brief Converts bars to clocks.
     * @param bars The time in bars.
     * @return The equivalent time in clocks.
     */
    static double barsToClocks(double bars)
    {
        return bars * 96.0;
    }
    
    /**
     * @brief Converts clocks to bars.
     * @param clocks The time in clocks.
     * @return The equivalent time in bars.
     */
    static double clocksToBars(double clocks)
    {
        return clocks / 96.0;
    }
    
    /**
     * @brief Converts seconds to beats based on the tempo.
     * @param tempo The tempo in BPM.
     * @param seconds The time in seconds.
     * @return The equivalent time in beats.
     */
    static double secondsToBeats(double tempo, double seconds)
    {
        return clocksToBeats(secondsToClocks(tempo, seconds));
    }
    
    /**
     * @brief Converts beats to seconds based on the tempo.
     * @param tempo The tempo in BPM.
     * @param beats The time in beats.
     * @return The equivalent time in seconds.
     */
    static double beatsToSeconds(double tempo, double beats)
    {
        jassert(tempo > 0.0);
        return clocksToSeconds(tempo, beatsToClocks(beats));
    }
    
    /**
     * @brief Converts seconds to beats using the current tempo.
     * @param seconds The time in seconds.
     * @return The equivalent time in beats.
     */
    double secondsToBeats(double seconds)
    {
        return secondsToBeats(getTempo(), seconds);
    }
 
    /**
     * @brief Converts beats to seconds using the current tempo.
     * @param beats The time in beats.
     * @return The equivalent time in seconds.
     */
    double beatsToSeconds(double beats)
    {
        return beatsToSeconds(getTempo(), beats);
    }
    
    /**
     * @brief Formats a time value in seconds as a human-readable string.
     * @param timeSec The time in seconds.
     * @return A formatted string representing the time.
     */
    static juce::String secondsToFormattedString(double timeSec)
    {
        int h = static_cast<int>(timeSec / (60.0 * 60.0));
        timeSec -= h * (60 * 60);
        
        int m = static_cast<int>(timeSec / (60.0));
        timeSec -= m * (60);
        
        int s = static_cast<int>(timeSec);
        
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
    /// A shared pointer to the `LinkEngine`.
    std::shared_ptr<audium::LinkEngine> linkEngine;

    /// The current tempo in beats per minute (BPM).
    double tempoBPM = 120.0;

    /// The sample rate in Hz.
    double sampleRate = 0.0;

    /// The buffer size in samples.
    int bufferSize = 0;
    
};

} // namespace audium

