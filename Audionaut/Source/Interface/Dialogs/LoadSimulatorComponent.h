//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#if JUCE_DEBUG

#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

/**
 * Debug builds only: the Load Simulator window. One row per track with a
 * slider for the extra busy time its render job burns, in percent of the
 * block period, and a "bursty" toggle that applies it one block in eight
 * (a stretch voice priming looks like that). The time counts as the job's
 * own, so the job order, the thread bars and the colours in the Processing
 * Load window all react as they would to real work. Everything resets when
 * the window closes.
 */
class LoadSimulatorComponent : public juce::Component,
                               private juce::Timer
{
public:
    explicit LoadSimulatorComponent (std::shared_ptr<audium::AudiumEngine> engine_) :
        engine (std::move (engine_))
    {
        hint.setText ("Extra busy time per track, in percent of the block period. "
                      "Bursty applies it one block in eight.", juce::dontSendNotification);
        hint.setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
        hint.setAlpha (0.6f);
        hint.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (hint);

        resetButton.setButtonText ("All off");
        resetButton.onClick = [this] { resetAll(); };
        addAndMakeVisible (resetButton);

        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&rows, false);
        viewport.setScrollBarsShown (true, false);

        rebuildRows();
        startTimerHz (4);   // follows track additions, removals and renames
    }

    ~LoadSimulatorComponent() override
    {
        stopTimer();
        resetAll();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        auto top = area.removeFromTop (rowHeight);
        resetButton.setBounds (top.removeFromRight (80).reduced (0, 2));
        hint.setBounds (top.withTrimmedRight (8));
        area.removeFromTop (8);
        viewport.setBounds (area);
        layoutRows();
    }

private:
    static constexpr int rowHeight = 24;

    struct Row
    {
        int trackId = -1;
        juce::Label name;
        juce::Slider share;
        juce::ToggleButton bursty;
    };

    void timerCallback() override
    {
        if (signature() != rowsSignature)
            rebuildRows();
    }

    /// The track list as the rows show it: ids, names and colours.
    juce::String signature() const
    {
        juce::String s;
        if (auto tracks = engine->getAudioTrackContainer())
            for (auto i = 0; i < tracks->getNumItems(); ++i)
                if (auto track = tracks->getAudioTrack (i))
                    s << i << ':' << track->getAudioTrackName() << ':' << track->getViewState().getColour().toString() << ';';
        return s;
    }

    void rebuildRows()
    {
        // keep what a track had set, by id, across a rebuild
        std::map<int, std::pair<double, bool>> kept;
        for (auto& row : rowList)
            kept[row->trackId] = { row->share.getValue(), row->bursty.getToggleState() };

        rowList.clear();
        rowsSignature = signature();

        auto tracks = engine->getAudioTrackContainer();
        if (tracks == nullptr)
            return;

        for (auto i = 0; i < tracks->getNumItems(); ++i)
        {
            auto track = tracks->getAudioTrack (i);
            if (track == nullptr)
                continue;

            auto row = std::make_unique<Row>();
            row->trackId = i;

            row->name.setText (track->getAudioTrackName(), juce::dontSendNotification);
            row->name.setColour (juce::Label::textColourId, track->getViewState().getColour());
            row->name.setMinimumHorizontalScale (1.0f);
            rows.addAndMakeVisible (row->name);

            row->share.setSliderStyle (juce::Slider::LinearHorizontal);
            row->share.setRange (0.0, 200.0, 1.0);
            row->share.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, rowHeight - 4);
            row->share.setTextValueSuffix (" %");
            row->share.setDoubleClickReturnValue (true, 0.0);
            rows.addAndMakeVisible (row->share);

            row->bursty.setButtonText ("bursty");
            rows.addAndMakeVisible (row->bursty);

            auto* rowPtr = row.get();
            row->share.onValueChange = [this, rowPtr] { apply (*rowPtr); };
            row->bursty.onClick = [this, rowPtr] { apply (*rowPtr); };

            if (auto it = kept.find (i); it != kept.end())
            {
                row->share.setValue (it->second.first, juce::dontSendNotification);
                row->bursty.setToggleState (it->second.second, juce::dontSendNotification);
            }
            apply (*row);

            rowList.push_back (std::move (row));
        }

        // a track that disappeared takes its load with it
        for (const auto& [id, state] : kept)
            if (id >= tracks->getNumItems())
                if (auto bus = engine->getAudioBusInterface())
                    bus->setSimulatedTrackLoad (id, 0.0f, false);

        layoutRows();
    }

    void layoutRows()
    {
        const auto width = viewport.getMaximumVisibleWidth();
        rows.setSize (width, static_cast<int> (rowList.size()) * rowHeight);

        auto y = 0;
        for (auto& row : rowList)
        {
            auto line = juce::Rectangle<int> (0, y, width, rowHeight);
            row->name.setBounds (line.removeFromLeft (120));
            row->bursty.setBounds (line.removeFromRight (70));
            row->share.setBounds (line);
            y += rowHeight;
        }
    }

    void apply (const Row& row)
    {
        if (auto bus = engine->getAudioBusInterface())
            bus->setSimulatedTrackLoad (row.trackId, static_cast<float> (row.share.getValue() / 100.0), row.bursty.getToggleState());
    }

    void resetAll()
    {
        for (auto& row : rowList)
        {
            row->share.setValue (0.0, juce::dontSendNotification);
            row->bursty.setToggleState (false, juce::dontSendNotification);
            apply (*row);
        }
    }

    std::shared_ptr<audium::AudiumEngine> engine;

    juce::Label hint;
    juce::TextButton resetButton;
    juce::Viewport viewport;
    juce::Component rows;
    std::vector<std::unique_ptr<Row>> rowList;
    juce::String rowsSignature;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoadSimulatorComponent)
};

#endif
