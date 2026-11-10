//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MixerPanel/MixerContext.h"
#include "Interface/Components/MixerPanel/MixerTrackGroupComponent.h"
#include "Interface/Components/MixerPanel/MixerSectionBar.h"
#include "Interface/Components/MixerPanel/MixerBusGroupComponent.h"
#include "Interface/Controls/PanelHeaderBar.h"

/**
 * The mixer at the bottom of the middle panel: one strip per channel,
 * grouped by track in arrangement order, scrolling horizontally, with the
 * section bar at the left that shows and hides strip sections (an app
 * preference; a toggle animates the section's room open or closed, and the
 * mixer's height with it), under the header bar that is the handle the owner lets the
 * mixer be dragged open and closed by (getHeader). Closed, the mixer is
 * the header bar alone: the owner sizes
 * it to the bar plus as much of the body as is open. The body's height is
 * its own (MixerLayout, from the shown sections and the send rows); the
 * owner hears about a change through onPreferredHeightChanged. The bus section (the aux buses, hidden by
 * setAuxBusesVisible, and the master) is pinned right of the scrolling strips.
 *
 * updateUI() re-reads the engine: when the structure changed (tracks,
 * channels, inserts, sends, aux buses) the groups are rebuilt, otherwise
 * refreshed in place, so a drag on a strip survives the refreshes its own
 * edits cause. The meters run on a timer while the mixer is showing, and
 * the section animation steps on it. The
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

    /// The height the mixer wants: the header bar, its strips with the shown sections, and the scroll bar.
    int getPreferredHeight() const;

    /// The part under the header bar that opens and closes: the strips and the scroll bar.
    int getBodyHeight() const { return getPreferredHeight() - MixerLayout::headerHeight; }

    /// Whether any of the body is on show (the owner sized the mixer beyond the header bar).
    bool isBodyShown() const noexcept { return viewport.isVisible(); }

    /// Whether the bus section shows the aux buses' strips before the master (an app preference, View > Aux Buses).
    bool areAuxBusesVisible() const noexcept { return auxBusesVisible; }
    void setAuxBusesVisible (bool visible);

    /// Fires when getPreferredHeight() changed (each frame of a section's animation, the send rows changed).
    std::function<void()> onPreferredHeightChanged;

    /// The band across the top: the owner wires its drag callbacks to the panel the mixer opens and closes in.
    PanelHeaderBar& getHeader() noexcept { return header; }

    void paint (juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

private:
    class Content;

    void timerCallback() override;
    void openEditor (const audium::InsertChainAddress& address, std::shared_ptr<audium::EffectSlot> slot, const juce::String& title);
    void closeEditorsWhoseSlotIsGone();
    juce::String structureSignature() const;

    void readPreferences();
    void writeSectionPreference();
    void rebuildBusGroup();
    int busColumnWidth() const;
    void preferredHeightMayHaveChanged();
    void animateSections();
    void stepSectionAnimation();
    void applyShown();

    MixerContext context;
    PanelHeaderBar header { "Mixer", TRANS ("Show or hide the mixer") };
    MixerSectionBar sectionBar;
    MixerLayout::Shown shown;             ///< how far each section is open right now: the bar's mask once settled, in between while animating
    MixerLayout::Shown animationFrom;     ///< the sections as they were when the last toggle began
    double animationStart = 0.0;
    bool animating = false;
    juce::Viewport viewport;
    std::unique_ptr<Content> content;
    std::unique_ptr<MixerBusGroupComponent> buses;   ///< right of the viewport, so it stays while the strips scroll
    bool auxBusesVisible = true;
    juce::String shownStructure;
    int lastPreferredHeight = 0;

    std::map<audium::EffectSlot*, std::unique_ptr<juce::Component>> editorWindows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerComponent)
};
