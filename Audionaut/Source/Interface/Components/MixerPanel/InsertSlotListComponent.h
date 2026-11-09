//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MixerPanel/MixerContext.h"

/**
 * The insert slots of one chain as a fixed column of rows in a strip, in
 * the manner of a Pro Tools insert section: a used slot shows its effect's
 * name (dim while bypassed), an unused slot is dark. Click opens a used
 * slot's editor or, on an empty slot, the list of effect types; right-click
 * a slot's menu; a drag within the list reorders. A chain longer than the
 * rows scrolls with the mouse wheel.
 */
class InsertSlotListComponent : public juce::Component,
                                public juce::DragAndDropTarget
{
public:
    static constexpr int rowHeight = 15;

    /// editorTitle names a slot's window ("Track 2 / ch 1 / EQ3") from its index.
    InsertSlotListComponent (MixerContext context, audium::InsertChainAddress address,
                             std::function<juce::String (int index)> editorTitle);
    ~InsertSlotListComponent() override;   // the row classes are complete in the .cpp only

    /// The list's height for a number of slot rows.
    static int heightForRows (int rows) { return rows * rowHeight; }

    /// Re-reads the chain; rebuilds the rows when the slots changed.
    void refresh();

    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDragMove (const SourceDetails& details) override;
    void itemDragExit (const SourceDetails& details) override;
    void itemDropped (const SourceDetails& details) override;

    /// The drag description of a slot row: {"mixerInsert": chainKey, "index": n}.
    static juce::var dragDescription (const juce::String& chainKey, int index);
    static juce::String keyFor (const audium::InsertChainAddress& address);

private:
    class SlotRow;
    class EmptyRow;

    int dropIndexFor (int y) const;
    void layoutRows();

    MixerContext context;
    audium::InsertChainAddress address;
    std::function<juce::String (int index)> editorTitle;
    juce::String chainKey;

    std::vector<std::unique_ptr<SlotRow>> slotRows;
    std::vector<std::unique_ptr<EmptyRow>> emptyRows;   // fill the column below the used slots
    std::vector<audium::EffectSlot*> shownSlots;   // what the rows were built from

    int firstVisibleRow = 0;
    int dropIndex = -1;   // where a dragged slot would land, -1 for no drag over the list

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InsertSlotListComponent)
};
