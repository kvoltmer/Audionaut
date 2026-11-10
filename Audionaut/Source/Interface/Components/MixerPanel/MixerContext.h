//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/ActionMessages.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Effects/InsertChain.h"
#include "Engine/Effects/MixerPlanner.h"
#include "Engine/Undo/UndoableEdit.h"
#include "Engine/Undo/UndoableEffectParameterAction.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

/**
 * What the mixer's parts share: the engine and the mixer's services. Copied
 * into menu callbacks, so it holds nothing that a rebuilt mixer invalidates
 * (openEditor guards its target itself).
 */
struct MixerContext
{
    std::shared_ptr<audium::AudiumEngine> engine;

    /// Opens, or raises, the editor window of a slot; the title says where the slot sits ("Track 2 / ch 1 / EQ3").
    std::function<void (const audium::InsertChainAddress&, std::shared_ptr<audium::EffectSlot>, const juce::String& title)> openEditor;

    audium::AudioTrackContainer& container() const { return *engine->getAudioTrackContainer(); }
    audium::MixerPlanner* planner() const { return container().getMixerPlanner(); }
    juce::UndoManager& undoManager() const { return *container().getUndoManager(); }

    /// The chain an address names right now, nullptr once its owner is gone.
    audium::InsertChain* chain (const audium::InsertChainAddress& address) const { return address.resolve (container()); }

    static juce::PopupMenu::Options menuOptions (juce::Component* target = nullptr)
    {
        auto options = juce::PopupMenu::Options().withStandardItemHeight (AudiumLookAndFeel::popupMenuItemHeight);
        return target != nullptr ? options.withTargetComponent (target) : options;
    }
};

/**
 * The edits the mixer makes to an insert chain, each one undo step, each
 * resolving the chain by address when it runs (a menu callback may fire
 * after the chain's owner is gone), and the menus built from them.
 */
namespace InsertChainActions
{
    using audium::InsertChain;
    using audium::InsertChainAddress;

    inline bool edit (const MixerContext& ctx, const InsertChainAddress& address, const juce::String& name,
                      const std::function<bool (InsertChain&)>& mutate)
    {
        auto* chain = ctx.chain (address);
        if (chain == nullptr)
            return false;

        return audium::applyAsUndoableEdit (ctx.container(), [&] { return mutate (*chain); }, name, false);   // the views re-read on its broadcast
    }

    inline bool add (const MixerContext& ctx, const InsertChainAddress& address, const std::string& typeId, int at = -1)
    {
        return edit (ctx, address, "Add Insert", [&] (InsertChain& chain) { return chain.add (typeId, at) != nullptr; });
    }

    inline bool remove (const MixerContext& ctx, const InsertChainAddress& address, int index)
    {
        return edit (ctx, address, "Remove Insert", [&] (InsertChain& chain) { return chain.remove (index); });
    }

    inline bool move (const MixerContext& ctx, const InsertChainAddress& address, int from, int to)
    {
        return edit (ctx, address, "Move Insert", [&] (InsertChain& chain) { return chain.move (from, to); });
    }

    inline bool setEnabled (const MixerContext& ctx, const InsertChainAddress& address, int index, bool enabled)
    {
        return edit (ctx, address, enabled ? "Enable Insert" : "Disable Insert",
                     [&] (InsertChain& chain) { return chain.setEnabled (index, enabled); });
    }

    inline bool replace (const MixerContext& ctx, const InsertChainAddress& address, int index, const std::string& typeId)
    {
        return edit (ctx, address, "Replace Insert", [&] (InsertChain& chain)
        {
            audium::MixerPlanner::ScopedBatch planBatch (ctx.planner());   // one plan for the swap
            return chain.remove (index) && chain.add (typeId, index) != nullptr;
        });
    }

    /// One item per registered effect type.
    inline juce::PopupMenu typesMenu (const MixerContext& ctx, std::function<void (const std::string& typeId)> onPick)
    {
        juce::PopupMenu menu;
        if (auto* planner = ctx.planner())
            for (const auto& type : planner->getRegistry().getTypes())
                menu.addItem (type.name, [onPick, id = type.id] { onPick (id); });
        return menu;
    }

    /// The context menu of one slot: Edit, Bypass or Enable, Replace, Move, Remove.
    inline juce::PopupMenu slotMenu (const MixerContext& ctx, const InsertChainAddress& address, int index, const juce::String& editorTitle)
    {
        juce::PopupMenu menu;
        auto* chain = ctx.chain (address);
        auto slot = chain != nullptr ? chain->getSlot (index) : nullptr;
        if (slot == nullptr)
            return menu;

        const auto enabled = slot->isEnabled();
        const auto last = chain->size() - 1;

        menu.addItem (TRANS ("Edit..."), [ctx, address, slot, editorTitle] { ctx.openEditor (address, slot, editorTitle); });
        menu.addItem (enabled ? TRANS ("Bypass") : TRANS ("Enable"), [ctx, address, index, enabled] { setEnabled (ctx, address, index, ! enabled); });
        menu.addSubMenu (TRANS ("Replace"), typesMenu (ctx, [ctx, address, index] (const std::string& id) { replace (ctx, address, index, id); }));
        menu.addSeparator();
        menu.addItem (TRANS ("Move Up"), index > 0, false, [ctx, address, index] { move (ctx, address, index, index - 1); });
        menu.addItem (TRANS ("Move Down"), index < last, false, [ctx, address, index] { move (ctx, address, index, index + 1); });
        menu.addSeparator();
        menu.addItem (TRANS ("Remove"), [ctx, address, index] { remove (ctx, address, index); });
        return menu;
    }

    /// The menu of a whole chain where there is no room for slot rows: one submenu per slot, then Add.
    inline juce::PopupMenu chainMenu (const MixerContext& ctx, const InsertChainAddress& address,
                                      const std::function<juce::String (int index)>& editorTitle)
    {
        juce::PopupMenu menu;
        if (auto* chain = ctx.chain (address)) {
            int index = 0;
            for (const auto& slot : chain->getSlots()) {
                auto name = juce::String (slot->getEffect().getType().name);
                if (! slot->isEnabled())
                    name += TRANS (" (bypassed)");
                menu.addSubMenu (name, slotMenu (ctx, address, index, editorTitle (index)));
                ++index;
            }
            if (index > 0)
                menu.addSeparator();
        }
        menu.addSubMenu (TRANS ("Add"), typesMenu (ctx, [ctx, address] (const std::string& id) { add (ctx, address, id); }));
        return menu;
    }
}
