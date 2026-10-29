//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/Core/DspLoadMeter.h"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/Playback/RenderThreadPool.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Interface/Controls/LoadMeter.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

/**
 * The Processing Load window: one bar per render thread, with numbers.
 *
 * The header's small meter shows the callback's deadline load - how much of
 * the block period passed before the callback returned. With tracks rendered
 * on worker threads that is the critical path, not the work: eight stretched
 * tracks cost the same CPU on one thread or on eight, but the callback time
 * drops. This window shows both. "Callback" is the deadline load with its
 * peak since the last frame; "Work" is the busy time summed over every render
 * thread, in percent of one block period, so it is the cost in cores; the
 * bars are each thread's own share of the period, the audio thread first
 * (its callback time less the time it waited for the workers), then the
 * workers, each coloured by the tracks it rendered in the last block.
 * Overruns and stalls count since the audio device started.
 */
class ProcessingLoadComponent : public juce::Component,
                                private juce::Timer
{
public:
    explicit ProcessingLoadComponent (std::shared_ptr<audium::AudiumEngine> engine_) :
        engine (std::move (engine_))
    {
        for (auto* readout : { &callback, &peak, &work, &threads, &overruns, &stalls })
        {
            readout->name.setJustificationType (juce::Justification::centredLeft);
            readout->name.setColour (juce::Label::textColourId, findColour (juce::Label::textColourId).withAlpha (0.7f));
            readout->value.setJustificationType (juce::Justification::centredRight);
            for (auto* label : { &readout->name, &readout->value })
            {
                label->setMinimumHorizontalScale (1.0f);
                label->setBorderSize (juce::BorderSize<int> (1, 0, 1, 0));   // text flush with the bar edges
                addAndMakeVisible (label);
            }
        }

        callback.name.setText ("Audio Callback", juce::dontSendNotification);
        peak.name.setText ("Peak", juce::dontSendNotification);
        work.name.setText ("Work of one core", juce::dontSendNotification);
        threads.name.setText ("Threads", juce::dontSendNotification);
        overruns.name.setText ("Overruns", juce::dontSendNotification);
        stalls.name.setText ("Stalls", juce::dontSendNotification);

        // the renderer records the job shares only while this window is open
        jobShares.resize (MAX_AUDIO_CHANNELS);
        setTrackSharesRecording (true);

        rebuildBars (1);
        startTimerHz (AudiumLookAndFeel::timerHz);
    }

    ~ProcessingLoadComponent() override
    {
        stopTimer();
        setTrackSharesRecording (false);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        auto readouts = area.removeFromBottom (rowHeight * 3);
        area.removeFromBottom (12);

        auto names = area.removeFromTop (rowHeight);
        auto percents = area.removeFromBottom (rowHeight);
        auto bars = area;

        const auto numBars = static_cast<int> (meters.size());
        if (numBars == 0)
            return;

        // bars share the width evenly, each no wider than a header meter is tall
        const auto column = bars.getWidth() / numBars;
        const auto barWidth = juce::jlimit (6, 24, column - 8);
        const auto barInset = (column - barWidth) / 2;

        for (auto i = 0; i < numBars; ++i)
        {
            const auto x = bars.getX() + i * column;
            nameLabels[static_cast<size_t> (i)]->setBounds (x, names.getY(), column, names.getHeight());
            meters[static_cast<size_t> (i)]->setBounds (x + barInset, bars.getY(), barWidth, bars.getHeight());
            percentLabels[static_cast<size_t> (i)]->setBounds (x, percents.getY(), column, percents.getHeight());
        }

        // the readouts sit under the bars in two narrow columns of three, the
        // callback's figures on the left, the pool's on the right, each a
        // label and a right-aligned value of fixed width; the left column
        // starts at the first bar's left edge, the right one ends at the last
        // bar's right edge
        const auto firstBarLeft = bars.getX() + barInset;
        const auto lastBarRight = bars.getX() + (numBars - 1) * column + barInset + barWidth;
        auto left = readouts.withX (firstBarLeft).withWidth (nameWidth + valueWidth);
        auto right = readouts.withX (lastBarRight - nameWidth - valueWidth).withWidth (nameWidth + valueWidth);
        for (auto* columnArea : { &left, &right })
        {
            auto* rows = columnArea == &left ? leftColumn : rightColumn;
            for (auto i = 0; i < 3; ++i)
            {
                auto row = columnArea->removeFromTop (rowHeight);
                rows[i]->name.setBounds (row.removeFromLeft (nameWidth));
                rows[i]->value.setBounds (row);
            }
        }
    }

private:
    static constexpr int rowHeight = 20;
    static constexpr int nameWidth = 116;
    static constexpr int valueWidth = 76;

    void timerCallback() override
    {
        auto device = engine->getLinkAudioDevice();
        auto bus = engine->getAudioBusInterface();
        if (device == nullptr || bus == nullptr)
            return;

        auto& dspLoad = device->getDspLoadMeter();
        auto& pool = bus->getRenderThreadPool();

        const auto numWorkers = pool.isRunning() ? pool.getNumWorkers() : 0;
        if (static_cast<int> (meters.size()) != numWorkers + 1)
        {
            rebuildBars (numWorkers + 1);
            resized();
        }

        // The engine's values follow the callbacks within a few blocks, which
        // is what the header meter and the overrun flash need; here they are
        // smoothed again over a few hundred milliseconds so that stretch
        // bursts read as a level, not a flicker. The peak is not smoothed.
        const auto peakNow = dspLoad.takePeak();
        shownCallback = smooth (shownCallback, dspLoad.getLoad());
        shownSpin = smooth (shownSpin, pool.getSpinLoad());
        const auto audioThread = juce::jmax (0.0f, shownCallback - shownSpin);

        auto workLoad = audioThread;
        for (auto i = 1; i <= numWorkers; ++i)
        {
            auto& shown = shownLoads[static_cast<size_t> (i)];
            shown = smooth (shown, pool.getThreadLoad (i));
            workLoad += shown;
        }

        updateSegments (bus.get(), numWorkers);

        meters[0]->setLoad (audioThread, peakNow);
        percentLabels[0]->setText (percent (audioThread), juce::dontSendNotification);

        for (auto i = 1; i <= numWorkers; ++i)
        {
            const auto load = shownLoads[static_cast<size_t> (i)];
            meters[static_cast<size_t> (i)]->setLoad (load, 0.0f);
            percentLabels[static_cast<size_t> (i)]->setText (percent (load), juce::dontSendNotification);
        }

        if (peakNow > shownPeak)
        {
            shownPeak = peakNow;
            peakHold = peakHoldFrames;
        }
        else if (peakHold > 0)
        {
            --peakHold;
        }
        else
        {
            shownPeak = shownCallback;
        }

        callback.value.setText (percent (shownCallback), juce::dontSendNotification);
        peak.value.setText (percent (shownPeak), juce::dontSendNotification);
        work.value.setText (percent (workLoad), juce::dontSendNotification);
        overruns.value.setText (juce::String (dspLoad.getOverrunCount()), juce::dontSendNotification);
        stalls.value.setText (juce::String (pool.getStallCount()), juce::dontSendNotification);

        const auto threadsText = pool.isRunning() ? juce::String (numWorkers + 1) : juce::String ("device stopped");
        threads.value.setText (threadsText, juce::dontSendNotification);
    }

    void setTrackSharesRecording (bool enabled)
    {
        if (auto bus = engine->getAudioBusInterface())
            bus->setTrackJobSharesEnabled (enabled);
    }

    /**
     * Reads the last block's job shares, smooths them per job and slot so
     * the picture holds still for a few hundred milliseconds, and hands
     * every bar its segments in the tracks' colours. A job may run on
     * another thread in the next block, so the colours can move between
     * bars; with jobs of clearly different cost they stay put.
     */
    void updateSegments (audium::AudioBusInterface* bus, int numWorkers)
    {
        const auto numJobs = bus->readTrackJobShares (jobShares.data(), static_cast<int> (jobShares.size()));
        const auto numSlots = numWorkers + 1;
        const auto stride = static_cast<size_t> (numSlots);

        // a job index stands for the same track from block to block as long
        // as the track list is unchanged; a change starts the smoothing over
        if (shownShares.size() != static_cast<size_t> (numJobs) * stride)
            shownShares.assign (static_cast<size_t> (numJobs) * stride, 0.0f);

        auto tracks = engine->getAudioTrackContainer();

        for (auto slot = 0; slot < numSlots; ++slot)
        {
            segmentScratch.clear();
            for (auto j = 0; j < numJobs; ++j)
            {
                auto& shown = shownShares[static_cast<size_t> (j) * stride + static_cast<size_t> (slot)];
                shown = smooth (shown, jobShares[static_cast<size_t> (j)].share[slot]);
                if (shown <= 0.0f)
                    continue;

                const auto id = jobShares[static_cast<size_t> (j)].trackId;
                auto track = tracks != nullptr && id >= 0 ? tracks->getAudioTrack (id) : nullptr;
                const auto colour = track != nullptr ? track->getViewState().getColour() : juce::Colours::grey;
                segmentScratch.push_back ({ shown, colour });
            }
            meters[static_cast<size_t> (slot)]->setSegments (segmentScratch.data(), static_cast<int> (segmentScratch.size()));
        }
    }

    void rebuildBars (int count)
    {
        meters.clear();
        percentLabels.clear();
        nameLabels.clear();
        shownLoads.assign (static_cast<size_t> (count), 0.0f);
        shownShares.clear();

        for (auto i = 0; i < count; ++i)
        {
            auto meter = std::make_unique<LoadMeter>();
            meter->setFallsInstantly (true);   // this window smooths the values itself; the segments are averages too
            addAndMakeVisible (*meter);
            meters.push_back (std::move (meter));

            auto value = std::make_unique<juce::Label>();
            value->setJustificationType (juce::Justification::centred);
            value->setFont (juce::FontOptions (12.0f));
            addAndMakeVisible (*value);
            percentLabels.push_back (std::move (value));

            auto name = std::make_unique<juce::Label> (juce::String(), i == 0 ? "audio" : juce::String (i));
            name->setJustificationType (juce::Justification::centred);
            name->setFont (juce::FontOptions (12.0f));
            name->setColour (juce::Label::textColourId, findColour (juce::Label::textColourId).withAlpha (0.7f));
            addAndMakeVisible (*name);
            nameLabels.push_back (std::move (name));
        }
    }

    static juce::String percent (float load)
    {
        return juce::String (juce::roundToInt (load * 100.0f)) + " %";
    }

    /// One display frame of smoothing: about a 300 ms time constant at the UI rate.
    static float smooth (float shown, float target)
    {
        return shown + displaySmoothing * (target - shown);
    }

    static constexpr float displaySmoothing = 1.0f / (0.3f * AudiumLookAndFeel::timerHz);

    static constexpr int peakHoldFrames = AudiumLookAndFeel::timerHz * 2;

    std::shared_ptr<audium::AudiumEngine> engine;

    struct Readout
    {
        juce::Label name, value;
    };

    Readout callback, peak, work, threads, overruns, stalls;
    Readout* leftColumn[3] = { &callback, &peak, &work };
    Readout* rightColumn[3] = { &threads, &overruns, &stalls };
    std::vector<std::unique_ptr<LoadMeter>> meters;
    std::vector<std::unique_ptr<juce::Label>> percentLabels;
    std::vector<std::unique_ptr<juce::Label>> nameLabels;

    std::vector<float> shownLoads;   // per bar, display-smoothed
    std::vector<audium::TrackJobShare> jobShares;    // the last block's, as read
    std::vector<float> shownShares;                  // [job * slots + slot], display-smoothed
    std::vector<LoadMeter::Segment> segmentScratch;
    float shownCallback = 0.0f;
    float shownSpin = 0.0f;
    float shownPeak = 0.0f;
    int peakHold = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessingLoadComponent)
};
