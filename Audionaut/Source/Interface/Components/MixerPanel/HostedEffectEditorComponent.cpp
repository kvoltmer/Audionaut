//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "HostedEffectEditorComponent.h"

#include <limits>

#include "Interface/ColourIds.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

using namespace audium;

HostedEffectEditorComponent::HostedEffectEditorComponent (MixerContext context_, InsertChainAddress address_, std::shared_ptr<EffectSlot> slot_) :
    context (std::move (context_)),
    address (address_),
    slot (std::move (slot_)),
    hosted (dynamic_cast<HostedEffect&> (slot->getEffect()))
{
    createEditor();
    followEditorSize();
    refresh();
    startTimerHz (10);
}

HostedEffectEditorComponent::~HostedEffectEditorComponent()
{
    stopTimer();
    editor.reset();   // before the slot, and with it the plugin the editor belongs to
}

void HostedEffectEditorComponent::createEditor()
{
    auto* instance = hosted.getInstance();
    if (instance == nullptr)
        return;   // a placeholder: the text says why (or that the plugin is on its way)

    if (instance->hasEditor())
        editor.reset (instance->createEditorAndMakeActive());
    if (editor == nullptr)
        editor = std::make_unique<juce::GenericAudioProcessorEditor> (*instance);

    addAndMakeVisible (editor.get());
}

bool HostedEffectEditorComponent::isUserResizable() const
{
    return editor != nullptr && editor->isResizable();
}

HostedEffectEditorComponent::SizeLimits HostedEffectEditorComponent::getSizeLimits() const
{
    SizeLimits limits { minimumWidth, 1, std::numeric_limits<int>::max() / 2, std::numeric_limits<int>::max() / 2 };
    if (auto* constrainer = editor != nullptr ? editor->getConstrainer() : nullptr)
    {
        limits.minimumWidth = juce::jmax (minimumWidth, constrainer->getMinimumWidth());
        limits.minimumHeight = juce::jmax (1, constrainer->getMinimumHeight());
        limits.maximumWidth = juce::jmax (limits.minimumWidth, juce::jmin (limits.maximumWidth, constrainer->getMaximumWidth()));
        limits.maximumHeight = juce::jmax (limits.minimumHeight, juce::jmin (limits.maximumHeight, constrainer->getMaximumHeight()));
    }
    return limits;
}

void HostedEffectEditorComponent::followEditorSize()
{
    // this component is the editor; without one, the room for the text
    juce::ScopedValueSetter<bool> guard (following, true);
    if (editor == nullptr)
        setSize (messageWidth, messageHeight);
    else
        setSize (juce::jmax (minimumWidth, editor->getWidth()), juce::jmax (1, editor->getHeight()));
}

void HostedEffectEditorComponent::childBoundsChanged (juce::Component* child)
{
    if (child == editor.get() && ! following)
        followEditorSize();   // the plugin resized its editor (or the user did, through the editor's own corner)
}

void HostedEffectEditorComponent::resized()
{
    if (editor == nullptr)
        return;

    const auto bounds = getLocalBounds();
    if (following || ! editor->isResizable()) {
        // the editor keeps its own size, centred when the window is wider than it
        editor->setTopLeftPosition ((bounds.getWidth() - editor->getWidth()) / 2, 0);
        return;
    }
    // the window was resized: its room is the editor's
    juce::ScopedValueSetter<bool> guard (following, true);
    editor->setBounds (bounds);
}

bool HostedEffectEditorComponent::isStillValid() const
{
    return slotIndex() >= 0;
}

int HostedEffectEditorComponent::slotIndex() const
{
    auto* chain = context.chain (address);
    if (chain == nullptr)
        return -1;

    const auto& slots = chain->getSlots();
    for (size_t i = 0; i < slots.size(); ++i)
        if (slots[i] == slot)
            return static_cast<int> (i);
    return -1;
}

void HostedEffectEditorComponent::refresh()
{
    if (editor != nullptr)
        editor->setAlpha (slot->isEnabled() ? 1.0f : 0.6f);
}

void HostedEffectEditorComponent::timerCallback()
{
    if (! isStillValid()) {
        stopTimer();
        if (onSlotGone != nullptr)
            juce::MessageManager::callAsync (onSlotGone);   // the owner deletes this component: not from inside its own timer
        return;
    }
    if (editor == nullptr && hosted.getInstance() != nullptr) {
        // the plugin arrived after the window opened on its placeholder
        createEditor();
        followEditorSize();
        repaint();
    }
    refresh();
}

void HostedEffectEditorComponent::paint (juce::Graphics& g)
{
    g.fillAll (findColour (audium::secondaryBackgroundColourId));

    if (editor != nullptr)
        return;

    // a placeholder: the plugin's name and why it is not running
    const auto textColour = findColour (audium::defaultTextColourId);
    auto area = getLocalBounds().reduced (margin);
    g.setColour (textColour);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (hosted.getDescription().name, area.removeFromTop (20), juce::Justification::centred, true);
    g.setColour (textColour.withAlpha (0.8f));
    g.setFont (juce::FontOptions (11.0f));
    const auto text = hosted.getStatus() == audium::HostedEffect::Status::loading ? TRANS ("Loading the plugin...")
                    : hosted.getError().isNotEmpty() ? hosted.getError()
                                                     : TRANS ("The plugin is not loaded.");
    g.drawFittedText (text, area, juce::Justification::centred, 3);
}
