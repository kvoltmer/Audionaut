//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MixerPanel/MixerContext.h"
#include "Interface/Components/MixerPanel/MixerLayout.h"
#include "Interface/Components/MixerPanel/InsertSlotListComponent.h"
#include "Interface/Controls/LevelMeter.h"
#include "Engine/Effects/AuxBus.h"
#include "Engine/Undo/UndoableSendAction.h"

/**
 * One return strip: an aux bus, or the master. It stands under its group's
 * band and is stacked like a track group with one channel strip, so the
 * faders line up across the mixer: nothing where a channel has its I/O, the
 * bus's inserts where the track group has its insert list, nothing where a
 * channel has its sends (a return has none), pan (the master's is its balance),
 * solo (an aux only) and mute, the fader on the console taper with a stereo meter beside it, and
 * the name bar: the aux's name, double-click to rename, right-click for
 * Rename / Remove; the master's reads "Main".
 *
 * Undo: a fader or pan drag is one step from the values before to the values
 * after, solo and mute one step each (UndoableAuxAction for an aux,
 * UndoableMasterAction for the master); rename and remove are container
 * edits.
 */
class MixerBusStripComponent : public juce::Component
{
public:
    static constexpr int masterBus = -1;

    /// @param auxId the aux bus's id, or masterBus
    MixerBusStripComponent (MixerContext context, int auxId);
    ~MixerBusStripComponent() override;

    int getAuxId() const noexcept { return auxId; }
    bool isMaster() const noexcept { return auxId == masterBus; }

    /// The colour of an aux bus's band and name bar, or the master's (masterBus).
    static juce::Colour busColour (int auxId) noexcept;

    /// Which sections to show (MixerLayout::Section flags); the hidden ones leave their room empty, as in the strips.
    void setSections (int sections);

    /// Re-reads the bus: name, values, inserts.
    void refresh();

    /// The meter, on the mixer's timer.
    void refreshLive();

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    class NameBar;

    bool shows (MixerLayout::Section section) const noexcept { return (sections & section) != 0; }
    std::shared_ptr<audium::AuxBus> bus() const;   ///< the aux bus, nullptr for the master or once it is gone
    audium::InsertChainAddress address() const;
    juce::String busName() const;

    void beginValueEdit();
    void endValueEdit (const juce::String& transactionName);
    void rename (const juce::String& newName);
    void showNameMenu();

    MixerContext context;
    const int auxId;
    int sections = MixerLayout::allSections;
    int stripTop = 0;   ///< where the strip's dark plate begins, under the inserts
    juce::Rectangle<int> panArea;   ///< the pan's caption and dial, empty while hidden

    std::unique_ptr<InsertSlotListComponent> inserts;
    juce::Slider pan;
    juce::TextButton solo { "S" }, mute { "M" };
    juce::Slider fader;
    LevelMeter meterLeft { true, false }, meterRight { true, false };
    std::unique_ptr<NameBar> name;

    audium::UndoableAuxAction::Values auxValuesAtEditStart;        ///< an aux's values when a drag or click began
    audium::UndoableMasterAction::Values masterValuesAtEditStart;   ///< the master's

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerBusStripComponent)
};
