//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

/**
 * @brief Vertical DSP load meter, styled like LevelMeter.
 *
 * Shows the smoothed load as a coloured bar (green -> orange -> red) on a
 * linear 0..100% scale of the audio callback's time budget, a held peak line,
 * and turns solid red for a moment whenever a callback overran its budget.
 */
class LoadMeter : public juce::Component
{
public:
    LoadMeter();
    ~LoadMeter() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Feed the meter from the UI timer.
        @param load  smoothed load, 1.0 == 100% of the callback budget
        @param peak  worst callback since the last call, same scale */
    void setLoad (float load, float peak);

    bool isSpiking() const { return spikeHold > 0; }

private:
    void redrawLevels();
    juce::Rectangle<int> getBarBounds() const;

    static constexpr int spacing = 1;
    static constexpr int peakHoldDuration = 50 * 4;     // UI frames, matches LevelMeter
    static constexpr int spikeHoldDuration = 60;        // UI frames (~1s at 60Hz)
    static constexpr float decrementPerFrame = 0.015f;

    float displayLoad = 0.0f;
    float peakLoad = 0.0f;
    int peakHold = 0;
    int spikeHold = 0;

    juce::Image levels;   // pre-rendered colour zones for the full bar

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoadMeter)
};
