//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/Effects/Hosting/HostedEffect.h"
#include "Interface/Components/MixerPanel/MixerContext.h"
#include "Interface/Components/MixerPanel/SlotEditor.h"

/**
 * The editor window content of a hosted plugin: the plugin's own editor,
 * nothing around it. A plugin without one gets JUCE's generic parameter
 * list; a placeholder (the plugin failed to load) gets the reason as text.
 * Bypass is the slot's (its menu in the mixer); a bypassed slot's editor
 * is dimmed.
 *
 * The component takes the size the plugin's editor wants and follows when
 * the editor changes it (the window follows the component); a plugin whose
 * editor lets the user resize it hands the window's room on to it.
 *
 * The slot is held strongly: the plugin instance outlives its editor, which
 * the destructor takes down first. Whether the slot is still in the project
 * is polled on a timer; once it is not, the owner is asked to close the
 * window.
 */
class HostedEffectEditorComponent : public juce::Component,
                                    public SlotEditor,
                                    private juce::Timer
{
public:
    static constexpr int minimumWidth = 200;
    static constexpr int messageWidth = 300;    ///< a placeholder's text box
    static constexpr int messageHeight = 90;
    static constexpr int margin = 10;

    HostedEffectEditorComponent (MixerContext context, audium::InsertChainAddress address, std::shared_ptr<audium::EffectSlot> slot);
    ~HostedEffectEditorComponent() override;

    bool isStillValid() const override;

    /// Whether the plugin's editor lets the user resize it; the window is then resizable too.
    bool isUserResizable() const;

    struct SizeLimits { int minimumWidth, minimumHeight, maximumWidth, maximumHeight; };
    /// The limits the plugin's editor sets: the window's resize limits.
    SizeLimits getSizeLimits() const;

    void resized() override;
    void paint (juce::Graphics& g) override;
    void childBoundsChanged (juce::Component* child) override;

private:
    void timerCallback() override;
    void refresh();
    int slotIndex() const;
    void createEditor();
    void followEditorSize();

    MixerContext context;
    audium::InsertChainAddress address;
    std::shared_ptr<audium::EffectSlot> slot;   ///< strong: the plugin lives as long as its editor
    audium::HostedEffect& hosted;

    std::unique_ptr<juce::AudioProcessorEditor> editor;
    bool following = false;   ///< sizing this to the editor right now: its bounds are not to be written back

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostedEffectEditorComponent)
};
