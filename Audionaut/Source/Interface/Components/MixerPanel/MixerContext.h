//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include <algorithm>

#include "Engine/AudiumEngine.h"
#include "Engine/ActionMessages.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Effects/InsertChain.h"
#include "Engine/Effects/MixerPlanner.h"
#include "Engine/Undo/UndoableEdit.h"
#include "Engine/Undo/UndoableEffectParameterAction.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/PlayList/PlayListScheduler.h"
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

    /// Opens the Manage Plugins window; unset where there is none (the headless builds).
    std::function<void()> openPluginManager;

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

    /// Adds at the end of the chain and opens the new slot's editor at once.
    inline bool addAndEdit (const MixerContext& ctx, const InsertChainAddress& address, const std::string& typeId,
                            const std::function<juce::String (int index)>& editorTitle)
    {
        if (! add (ctx, address, typeId))
            return false;
        auto* chain = ctx.chain (address);
        if (chain == nullptr || chain->size() == 0)
            return true;
        const auto index = chain->size() - 1;
        if (ctx.openEditor != nullptr)
            ctx.openEditor (address, chain->getSlot (index), editorTitle (index));
        return true;
    }

    inline bool remove (const MixerContext& ctx, const InsertChainAddress& address, int index)
    {
        return edit (ctx, address, "Remove Insert", [&] (InsertChain& chain) { return chain.remove (index); });
    }

    inline bool move (const MixerContext& ctx, const InsertChainAddress& address, int from, int to)
    {
        return edit (ctx, address, "Move Insert", [&] (InsertChain& chain) { return chain.move (from, to); });
    }

    /// Drops an element the chain keeps for a plugin this machine lacks (position among the chain's unresolved elements).
    inline bool removeUnresolved (const MixerContext& ctx, const InsertChainAddress& address, int position)
    {
        return edit (ctx, address, "Remove Missing Insert", [&] (InsertChain& chain) { return chain.removeUnresolved (position); });
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
            if (chain.getSlot (index) == nullptr)
                return false;
            audium::MixerPlanner::ScopedBatch planBatch (ctx.planner());   // one plan for the swap
            // the new one goes in ahead of the old, so a type that cannot be made leaves the chain as it was
            return chain.add (typeId, index) != nullptr && chain.remove (index + 1);
        });
    }

    /// The effect types by category, a submenu each, in the order the registry first lists one of the
    /// category (the built-ins' categories first, then the plugins by manufacturer); a type without a
    /// category goes on the top level. Last, where the app has one, the Manage Plugins window.
    inline juce::PopupMenu typesMenu (const MixerContext& ctx, std::function<void (const std::string& typeId)> onPick)
    {
        juce::PopupMenu menu;
        auto* planner = ctx.planner();
        if (planner == nullptr)
            return menu;

        const auto& types = planner->getRegistry().getTypes();
        std::vector<std::string> categories;
        for (const auto& type : types)
            if (! type.category.empty() && std::find (categories.begin(), categories.end(), type.category) == categories.end())
                categories.push_back (type.category);

        for (const auto& category : categories)
        {
            juce::PopupMenu submenu;
            for (const auto& type : types)
                if (type.category == category)
                    submenu.addItem (type.name, [onPick, id = type.id] { onPick (id); });
            menu.addSubMenu (juce::String (category), submenu);
        }
        for (const auto& type : types)
            if (type.category.empty())
                menu.addItem (type.name, [onPick, id = type.id] { onPick (id); });

        if (ctx.openPluginManager != nullptr) {
            menu.addSeparator();
            menu.addItem (TRANS ("Manage Plugins..."), [open = ctx.openPluginManager] { open(); });
        }
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
        menu.addSubMenu (TRANS ("Add"), typesMenu (ctx, [ctx, address, editorTitle] (const std::string& id) { addAndEdit (ctx, address, id, editorTitle); }));
        return menu;
    }
}

/**
 * What a channel strip does to its bus channel, in the mixer and the arrangement
 * alike: the pan dial's set-up, the record arm with its rules (an input must be
 * there, the arming is one undo step, a running armed transport starts or stops
 * the take), and the input / output routing menu with its one-step route change.
 */
namespace ChannelActions
{
    inline bool audioInputAvailable (const std::shared_ptr<audium::AudiumEngine>& engine,
                                     const std::shared_ptr<audium::AudioTrack>& audioTrack, int channelNumber);
    inline void setRoutingChannel (const std::shared_ptr<audium::AudioTrack>& audioTrack, int channelNumber, bool isInput, int newChannel);

    inline void setRoutingChannel (const std::shared_ptr<audium::AudioTrack>& audioTrack, int channelNumber, bool isInput, int newChannel)
    {
        audium::applyAsUndoableEdit(audioTrack->getAudioTrackContainer(), [&]
        {
            auto channel = audioTrack->getChannel (channelNumber);
            if (channel == nullptr)
                return false;
            if (isInput)
                channel->setInputChannel(newChannel);
            else
                channel->setOutputChannel(newChannel);
            return true;
        },
        "Set channel routing", false);   // the strips re-read the route on its broadcast
    }

    inline juce::PopupMenu routingMenu (const std::shared_ptr<audium::AudiumEngine>& engine,
                                                   const std::shared_ptr<audium::AudioTrack>& audioTrack, int channelNumber, bool input)
    {
        const auto channelData = audioTrack->getChannelData (channelNumber);
        const auto selected = input ? channelData.inputChannel : channelData.outputChannel;

        StringArray names;
        if (auto* device = engine->getAudioDeviceManager()->getCurrentAudioDevice())
            names = input ? device->getInputChannelNames() : device->getOutputChannelNames();

        const auto pick = [audioTrack, channelNumber, input] (int route)
        {
            return [audioTrack, channelNumber, input, route] { setRoutingChannel (audioTrack, channelNumber, input, route); };
        };

        PopupMenu menu;
        menu.addItem (input ? TRANS("Default (input ") + String (channelNumber + 1) + ")" : TRANS("Main Bus"), true, selected < 0, pick (-1));
        for (auto i = 0; i < names.size(); ++i) {
            const auto name = names[i].trim();
            menu.addItem (name.isNotEmpty() ? name : String (i + 1), true, selected == i, pick (i));
        }
        if (selected >= names.size())
            menu.addItem ((input ? TRANS("Input ") : TRANS("Output ")) + String (selected + 1) + TRANS(" (not available)"), false, true, nullptr);
        return menu;
    }

    inline void configurePanSlider (juce::Slider* slider)
    {
        slider->setSliderStyle(juce::Slider::LinearBar);
        slider->setColour(Slider::textBoxTextColourId, juce::Colours::white);
        slider->setColour(Slider::trackColourId, juce::Colours::grey.withAlpha(0.5f));

        slider->setDoubleClickReturnValue(true, 0.0);

        slider->setNormalisableRange(NormalisableRange<double>(-1.0, 1.0));

        slider->textFromValueFunction = [](auto val) {
            auto intVal = static_cast<int>(val * 50.0);
            if (intVal == 0) {
                return String("C");
            }
            else if (intVal < 0) {
                return String(abs(intVal)) + String(" L");
            }
            else if (intVal > 0) {
                return String(intVal) + String(" R");
            }
            return String("");
        };
        slider->updateText();
    }

    inline bool audioInputAvailable (const std::shared_ptr<audium::AudiumEngine>& engine,
                                                const std::shared_ptr<audium::AudioTrack>& audioTrack, int channelNumber)
    {
        auto currentDevice = engine->getAudioDeviceManager()->getCurrentAudioDevice();
        if (currentDevice == nullptr)
            return false;

        auto channelData = audioTrack->getChannelData(channelNumber);
        auto effectiveInput = channelData.inputChannel >= 0 ? channelData.inputChannel : channelNumber;

        return effectiveInput < currentDevice->getActiveInputChannels().countNumberOfSetBits();
    }

    inline void setRecordEnabled (const std::shared_ptr<audium::AudiumEngine>& engine,
                                             const std::shared_ptr<audium::AudioTrack>& audioTrack, int channelNumber, bool bEnabled)
    {
        auto& container = *engine->getAudioTrackContainer();
        auto stepRecorded = false;

        if (audioInputAvailable(engine, audioTrack, channelNumber)) {

            // Undo: store old state
            auto action = bEnabled ? std::make_unique<audium::UndoableContainerAction>(container, false) : nullptr;

            if (ModifierKeys::currentModifiers.isShiftDown())
                audioTrack->setRecordEnabled(-1, bEnabled);
            else
                audioTrack->setRecordEnabled(channelNumber, bEnabled);

            auto scheduler = engine->getPlayListScheduler();

            // start recording on the fly ?
            if (scheduler->isPlaying() &&
                scheduler->isRecordingArmed()) {

                if (bEnabled)
                    scheduler->startRecording(channelNumber + audioTrack->getChannelOffset());
                else
                    scheduler->stopRecording(channelNumber + audioTrack->getChannelOffset());
            }

            // Undo: store new state; applying it tells the views
            if (action != nullptr) {
                action->storeNewState();
                container.getUndoManager()->perform(action.release(), "Record");
                container.getUndoManager()->beginNewTransaction();
                stepRecorded = true;
            }
        }
        else {
            if (bEnabled) {
                juce::NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                            "No audio input available at this channel.",
                                                            "Please check: Settings ... -> Audio Device Settings");
            }
        }

        // a disarm records no step and a refused arm changed nothing, so no broadcast came from the container:
        // the arrangement's channel strip and the mixer strip both show the arm, and the one not clicked (or a
        // button the engine refused) re-reads the channel's state here
        if (! stepRecorded)
            container.sendActionMessage (audium::updateChannelsAction);
    }
}
