//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"

class RegionSelector;

/**
 * @class ClipOverlayBase
 * @brief Shared machinery for transparent in-arrangement overlay controls.
 *
 * An overlay control floats over one clip while an edit session is pending
 * (the Auto Edit preview, the stretch editor). This base carries everything
 * those sessions have in common, so a concrete overlay only supplies its
 * widgets and its edit semantics:
 *
 * - **Positioning**: keeps itself centred in the viewport-visible part of
 *   the clip; holds still inside a dead zone while the clip drifts, glides
 *   to re-committed spots, and snaps on first show. Fades away while a zoom
 *   reshapes the clip and comes back once the zoom has settled.
 * - **Session plumbing**: visibility is the session boundary. Showing takes
 *   over the arrangement - the RegionSelector is disabled for the whole
 *   session (re-asserted in mouseEnter, because hover controls hand it back
 *   on their own mouseExit), the selection is followed, keyboard focus is
 *   grabbed. Hiding releases all of it.
 * - **Escape** dismisses by clearing the engine-side state that drives the
 *   overlay's visibility (see dismissOverlay) - never by hiding directly.
 * - **Looks**: the scrim panel, the shared button/label metrics, and the
 *   icon-button factory with the pinned 24-unit design box.
 *
 * A subclass implements the pure virtuals, adds its widgets in its
 * constructor, lays them out in resized(), and reacts to the shown/hidden
 * hooks. It holds no clip pointer: the base stores (trackId,
 * playlistItemId) and everything is re-resolved from the engine, because
 * clip components are recycled or replaced underneath.
 */
class ClipOverlayBase : public juce::Component,
                        public juce::ChangeListener,
                        private juce::ActionListener,
                        private juce::Timer
{
public:
    ClipOverlayBase(std::shared_ptr<audium::AudiumEngine> audiumEngine,
                    std::shared_ptr<RegionSelector> regionSelector);
    ~ClipOverlayBase() override;

    /** @brief Names the clip this overlay edits. */
    void setTarget(int trackId, int playlistItemId);

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void parentSizeChanged() override;

protected:
    // ------------------------------------------------------------------
    // The subclass contract

    /// Width with every widget visible.
    virtual int getPreferredWidth() const = 0;

    /// Narrowest still-useful width (secondary widgets sit out; see
    /// resized() in the subclasses). Below it the overlay hides entirely
    /// until scrolling or zooming makes room.
    virtual int getMinimumWidth() const = 0;

    /// Ends the session by clearing whatever engine-side state shows this
    /// overlay - visibility follows the data, so this is how Escape (and
    /// the toggling command) dismiss.
    virtual void dismissOverlay() = 0;

    /// The session began: the overlay just became visible. Sync widgets
    /// from the engine here. Runs before the base takes its listeners.
    virtual void overlayShown() {}

    /// The session ended: the overlay was hidden. Commit pending work
    /// here. Runs before the base releases its listeners. A subclass whose
    /// session must also survive destruction (a rebuild destroys visible
    /// overlays without a visibilityChanged) commits from its destructor
    /// too - that runs before the base destructor's cleanup.
    virtual void overlayHidden() {}

    /// Arrangement broadcasts beyond the scroll the base already handles.
    virtual void overlayActionReceived (const juce::String& message) { juce::ignoreUnused (message); }

    /// Labels under the widgets, on the scrim. The default draws every
    /// visible child DrawableButton's name beneath it.
    virtual void paintLabels (juce::Graphics& g);

    // ------------------------------------------------------------------
    // Shared metrics - a row of icon buttons sized like the header's
    // transport buttons (see HeaderComponent::resized), labels painted
    // underneath on the scrim.
    static constexpr int buttonWidth = 35;
    static constexpr int buttonHeight = 20;
    static constexpr int labelHeight = 12;
    static constexpr float labelFontHeight = 10.0f;
    static constexpr int verticalPadding = 6;
    static constexpr int controlHeight = verticalPadding + buttonHeight + labelHeight + verticalPadding;
    static constexpr int stepGap = 10;
    static constexpr int gap = 16;
    // Generous enough that the close chip (which reaches padding-ish far
    // into the scrim) stays clear of the first widget.
    static constexpr int padding = 12;

    // The round close chip straddles the scrim's top-left corner. The
    // component is grown by the overhang - on both sides, so the scrim
    // keeps its size and stays horizontally centred - and the scrim is
    // drawn inset; see getContentArea().
    static constexpr int closeButtonSize = 16;
    static constexpr int closeButtonOverhang = 6;

    /**
     * Icon paths share a 24-unit design box so all icons scale identically.
     * DrawableButton fits the drawable's own bounds into the image area, so
     * the invisible corner subpaths pin every path's bounds to the full
     * box - without them a lone minus bar would be blown up to fill the
     * area.
     */
    static void pinToDesignBox (juce::Path& path);

    /// Minus / plus bars in the design box - shared by Less/More and the
    /// semitone steppers.
    static juce::Path minusIconPath();
    static juce::Path plusIconPath();

    /// Apply: check mark - shared by both overlays' confirm buttons.
    static juce::Path checkIconPath();

    /**
     * Where the scrim and the widgets live: the local bounds minus the
     * close chip's overhang (left, right and top - symmetric horizontally
     * so the scrim stays centred). Subclasses lay their widgets out inside
     * this (and call the base resized() first, which places the chip).
     */
    juce::Rectangle<int> getContentArea() const;

    /**
     * An icon button looking like the header's transport buttons - same
     * style and the stop button's grey. The label is not part of the
     * button - paintLabels() draws it underneath on the scrim. The disabled
     * image is handed over explicitly because the look-and-feel dims only
     * the background of a disabled button, not its drawable.
     */
    static std::unique_ptr<juce::DrawableButton> makeIconButton (const juce::String& label,
                                                                 const juce::Path& iconPath);

    /// One label under @p component on the scrim, dimmed when disabled.
    void drawLabel (juce::Graphics& g, const juce::Component& component,
                    const juce::String& text, bool enabled) const;

    /// Centres the control in the visible part of the clip (see class
    /// comment). The base calls it; subclasses rarely need to.
    void updatePosition();

    /**
     * Announce that an edit is about to re-lay the clip out (a live
     * stretch or drag changes the clip's width). Parent resizes within a
     * short window are then followed in place instead of running the zoom
     * fade - without this every live tweak would blink the overlay away
     * and back. The base arms this itself on every arrangement-update
     * broadcast, so drags on the clip are covered too.
     */
    void expectParentResize();

    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<RegionSelector> regionSelector;

    int trackId = -1;
    int playlistItemId = -1;

private:
    void actionListenerCallback (const juce::String& message) override;
    void timerCallback() override;
    void hideForZoom();
    void restoreAfterZoom();
    void releaseRegionSelector();

    // Where the control is headed, in viewport coordinates. Stillness means
    // holding a position on screen - in clip coordinates the clip itself
    // moves with every scroll and zoom, so anchoring there would wobble.
    juce::Rectangle<int> committedTarget;
    bool snapOnNextUpdate = true;
    bool hiddenForZoom = false;

    // Parent resizes up to this moment are edit-driven relayouts to follow
    // in place; afterwards a resize means a zoom (see expectParentResize).
    juce::uint32 followResizesUntil = 0;

    std::unique_ptr<juce::DrawableButton> closeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipOverlayBase)
};
