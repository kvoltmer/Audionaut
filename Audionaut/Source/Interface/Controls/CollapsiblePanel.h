//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

/**
 * The state of a panel the user opens, closes and resizes at a seam: the
 * mixer under the arrangement, the right panel.
 *
 * It keeps the user's choice (open or closed, and the size they dragged it
 * to) apart from what is on screen: the shown size follows the choice with
 * a short animation whenever the panel opens or closes. That includes the
 * middle of a seam drag, like Xcode's debug area: dragging the seam well past
 * the minimum collapses the panel, dragging back brings it back, and the
 * release decides. A panel closed by a drag remembers the size it had before.
 *
 * The owner lays itself out from getShownSize() (0 = closed), supplies the
 * maximum size from its own bounds, and wires a PanelResizerBar's callbacks
 * to dragStarted() / dragged() / dragEnded() / resetToDefault(). onRelayout
 * fires whenever the shown size changed, onUserChange whenever the user's
 * choice did (the moment to persist it).
 */
class CollapsiblePanel : private juce::Timer
{
public:
    CollapsiblePanel (int minimumSize, int defaultSize) :
        minimum (minimumSize),
        preferred (defaultSize),
        size (defaultSize)
    {
    }

    ~CollapsiblePanel() override { stopTimer(); }

    std::function<int()> maximumSize;      ///< the owner's limit right now; unset = none
    std::function<void()> onRelayout;      ///< the shown size changed
    std::function<void()> onUserChange;    ///< open state or chosen size changed by the user

    //==============================================================================
    bool isOpen() const noexcept { return open; }
    int getSize() const noexcept { return size; }   ///< the user's size, kept while closed
    bool isDragging() const noexcept { return dragging; }

    /// What to lay out right now: 0 when closed, the chosen size (as far as the owner's maximum allows) when open and settled, in between while animating.
    int getShownSize() const noexcept
    {
        if (isTimerRunning())
            return juce::jmin (juce::roundToInt (shown), maximumNow());

        return open ? clampedSize() : 0;
    }

    //==============================================================================
    /// From the preferences: no animation, no onUserChange.
    void restore (bool shouldBeOpen, int restoredSize)
    {
        open = shouldBeOpen;
        size = juce::jmax (minimum, restoredSize);
        settle();
    }

    /// A panel whose size is its content's, not the user's: the seam then only closes and reopens it.
    void setFixedSize (int newSize)
    {
        minimum = preferred = size = juce::jmax (1, newSize);
        if (open && ! dragging && ! isTimerRunning())
            settle();
    }

    /// A button or menu command.
    void setOpen (bool shouldBeOpen)
    {
        if (open == shouldBeOpen)
            return;

        changeAnimated (shouldBeOpen, size);
        juce::NullCheckedInvocation::invoke (onUserChange);
    }

    //==============================================================================
    // The seam. Distances are the mouse's travel from the mouse-down point along
    // the seam's axis, positive towards the panel (which makes it smaller).

    void dragStarted()
    {
        dragging = true;
        openBeforeDrag = open;
        sizeBeforeDrag = size;
        shownAtDragStart = getShownSize();
        closedByThisDrag = false;
    }

    void dragged (int distance)
    {
        const auto wanted = shownAtDragStart - distance;

        // hysteresis: collapse well under the minimum, come back a little earlier
        if (open && wanted < minimum - closeMargin) {
            closedByThisDrag = true;
            changeAnimated (false, size);
        }
        else if (! open && closedByThisDrag && wanted >= minimum - closeMargin / 2) {
            changeAnimated (true, minimum);
        }
        else if (open) {
            size = juce::jlimit (minimum, maximumNow(), wanted);
            // mid-animation (a panel coming back under the mouse) the target follows, otherwise the seam is the mouse
            if (isTimerRunning())
                animate (getShownSize(), clampedSize());
            else
                settle();
        }
    }

    void dragEnded()
    {
        dragging = false;

        if (! open)
            size = sizeBeforeDrag;   // reopens as large as it was before this drag

        if (open != openBeforeDrag || size != sizeBeforeDrag)
            juce::NullCheckedInvocation::invoke (onUserChange);

        // the seam of a panel closed by this drag goes with it (the owner keeps it through the drag only)
        juce::NullCheckedInvocation::invoke (onRelayout);
    }

    /// Double-click on the seam: the default size, open.
    void resetToDefault()
    {
        changeAnimated (true, preferred);
        juce::NullCheckedInvocation::invoke (onUserChange);
    }

private:
    static constexpr int closeMargin = 40;           ///< dragged this far under the minimum, the panel collapses
    static constexpr double animationMilliseconds = 200.0;

    int maximumNow() const noexcept
    {
        return maximumSize != nullptr ? juce::jmax (minimum, maximumSize()) : std::numeric_limits<int>::max();
    }

    int clampedSize() const noexcept { return juce::jlimit (minimum, maximumNow(), size); }

    /// No animation: the shown size is the state, right away.
    void settle()
    {
        stopTimer();
        juce::NullCheckedInvocation::invoke (onRelayout);
    }

    /// A new state, eased in from what is on screen now (read before the state changes, or there is nothing to ease).
    void changeAnimated (bool newOpen, int newSize)
    {
        const auto from = getShownSize();
        open = newOpen;
        size = juce::jmax (minimum, newSize);
        animate (from, open ? clampedSize() : 0);
    }

    void animate (int from, int to)
    {
        if (from == to) {
            settle();
            return;
        }

        shown = from;
        animationFrom = from;
        animationTo = to;
        animationStart = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (60);
    }

    void timerCallback() override
    {
        const auto t = juce::jlimit (0.0, 1.0, (juce::Time::getMillisecondCounterHiRes() - animationStart) / animationMilliseconds);
        const auto eased = 1.0 - std::pow (1.0 - t, 3.0);   // fast out, settling in
        shown = animationFrom + (animationTo - animationFrom) * eased;

        if (t >= 1.0) {
            shown = animationTo;
            stopTimer();
        }

        juce::NullCheckedInvocation::invoke (onRelayout);
    }

    int minimum;
    int preferred;

    bool open = false;
    int size;

    double shown = 0.0;   ///< the animated size, meaningful while the timer runs
    double animationFrom = 0.0, animationTo = 0.0, animationStart = 0.0;

    bool dragging = false;
    bool openBeforeDrag = false;
    bool closedByThisDrag = false;
    int sizeBeforeDrag = 0;
    int shownAtDragStart = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CollapsiblePanel)
};
