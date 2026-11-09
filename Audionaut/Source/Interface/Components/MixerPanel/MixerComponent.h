//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MixerPanel/MixerContext.h"
#include "Interface/Components/MixerPanel/MixerTrackGroupComponent.h"
#include "Interface/Components/MixerPanel/MixerSectionBar.h"

/**
 * The mixer at the bottom of the middle panel: one strip per channel,
 * grouped by track in arrangement order, scrolling horizontally, with the
 * section bar at the left that shows and hides strip sections (an app
 * preference). Its height is its own (MixerLayout, from the shown sections
 * and the send rows); the owner hears about a change through
 * onPreferredHeightChanged. (The aux buses and the master follow in the next
 * step of phase 5.)
 *
 * updateUI() re-reads the engine: when the structure changed (tracks,
 * channels, inserts, sends, aux buses) the groups are rebuilt, otherwise
 * refreshed in place, so a drag on a strip survives the refreshes its own
 * edits cause. The meters run on a timer while the mixer is showing. The
 * effect editor windows are owned here, one per slot, and closed once
 * their slot is gone.
 */
class MixerComponent : public juce::Component,
                       private juce::Timer
{
public:
    explicit MixerComponent (std::shared_ptr<audium::AudiumEngine> engine);
    ~MixerComponent() override;

    /// Re-reads the engine: tracks, channels and their strips.
    void updateUI();

    /// The height the mixer wants: its strips with the shown sections, and the scroll bar.
    int getPreferredHeight() const;

    /// Fires when getPreferredHeight() changed (a section toggled, the send rows changed).
    std::function<void()> onPreferredHeightChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

private:
    class Content;

    void timerCallback() override;
    void openEditor (const audium::InsertChainAddress& address, std::shared_ptr<audium::EffectSlot> slot, const juce::String& title);
    void closeEditorsWhoseSlotIsGone();
    juce::String structureSignature() const;

    void readSectionPreference();
    void writeSectionPreference();
    void preferredHeightMayHaveChanged();

    MixerContext context;
    MixerSectionBar sectionBar;
    juce::Viewport viewport;
    std::unique_ptr<Content> content;
    juce::String shownStructure;
    int lastPreferredHeight = 0;

    std::map<audium::EffectSlot*, std::unique_ptr<juce::Component>> editorWindows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerComponent)
};
