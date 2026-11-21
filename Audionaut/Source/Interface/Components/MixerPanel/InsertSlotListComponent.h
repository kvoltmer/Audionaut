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
 * name - on a tint of the colour of its track or bus while the effect
 * runs, plain and dim while it is bypassed - and an unused slot is dark.
 * Under the mouse a used slot shows three controls in the name's place:
 * bypass, the editor (a click opens its window, the next closes it), and
 * a menu that replaces the slot with another effect type or removes it.
 * A click on an empty slot lists the effect types; a drag within the list
 * reorders. A chain longer than the rows scrolls with the mouse wheel.
 *
 * A slot whose plugin is not loaded has nothing to bypass and no editor:
 * under the mouse it shows the menu alone, beside its name.
 *
 * An element the chain keeps for a plugin this machine lacks shows as a
 * dimmed row with a marker at the place it was saved in, named after the
 * plugin; its menu removes it. A hosted plugin that is not running (failed
 * to load, refuses the chain's channel count) carries the marker too, the
 * reason as its tooltip.
 */
class InsertSlotListComponent : public juce::Component,
                                public juce::DragAndDropTarget
{
public:
    static constexpr int rowHeight = 18;   ///< as high as a track's name band (MixerLayout::bandHeight)

    /// colour is that of the chain's track or bus, asked for at each paint as a track's can change.
    InsertSlotListComponent (MixerContext context, audium::InsertChainAddress address,
                             std::function<juce::Colour()> colour);
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
    class MissingRow;
    class EmptyRow;

    /// One row of the column: a slot, or an unresolved element at its saved place.
    struct Entry
    {
        std::shared_ptr<audium::EffectSlot> slot;   // null for a missing plugin's element
        int slotIndex = -1;                           // in the chain, for a slot
        int unresolvedPosition = -1;                  // among the chain's unresolved elements, for a missing one
        bool operator== (const Entry& other) const { return slot == other.slot && unresolvedPosition == other.unresolvedPosition; }
    };
    std::vector<Entry> readEntries() const;

    int dropRowFor (int y) const;
    int slotIndexAtRow (int row) const;
    void layoutRows();

    MixerContext context;
    audium::InsertChainAddress address;
    std::function<juce::Colour()> colour;
    juce::String chainKey;

    std::vector<std::unique_ptr<juce::Component>> rows;   // the used slots and the missing elements, in the chain's order
    std::vector<std::unique_ptr<EmptyRow>> emptyRows;      // fill the column below them
    std::vector<Entry> shown;   // what the rows were built from

    int firstVisibleRow = 0;
    int dropRow = -1;   // the row a dragged slot would land before, -1 for no drag over the list

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InsertSlotListComponent)
};
