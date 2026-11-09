//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "EffectEditorComponent.h"

#include "Interface/ColourIds.h"

using namespace audium;

EffectEditorComponent::EffectEditorComponent (MixerContext context_, InsertChainAddress address_, std::shared_ptr<EffectSlot> slot) :
    context (std::move (context_)),
    address (address_),
    slotRef (slot)
{
    jassert (slot != nullptr);
    auto& effect = slot->getEffect();
    effectName = effect.getType().name;

    bypassToggle = std::make_unique<juce::ToggleButton> (TRANS ("Bypass"));
    addAndMakeVisible (bypassToggle.get());
    bypassToggle->onClick = [this]
    {
        const auto index = slotIndex();
        if (index >= 0)
            InsertChainActions::setEnabled (context, address, index, ! bypassToggle->getToggleState());
    };

    const auto parameters = effect.getParameters();
    for (int i = 0; i < static_cast<int> (parameters.size()); ++i)
    {
        const auto& info = parameters[static_cast<size_t> (i)];
        auto row = std::make_unique<Row>();
        row->parameter = i;

        row->label = std::make_unique<juce::Label> (juce::String(), juce::String (info.name));
        row->label->setJustificationType (juce::Justification::centredLeft);
        row->label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (row->label.get());

        if (info.boolean)
        {
            row->toggle = std::make_unique<juce::ToggleButton>();
            addAndMakeVisible (row->toggle.get());
            row->toggle->onClick = [this, rowPtr = row.get()]
            {
                if (auto slot = slotRef.lock())
                    commit (rowPtr->parameter, slot->getEffect().getParameter (rowPtr->parameter), rowPtr->toggle->getToggleState() ? 1.0f : 0.0f);
            };
        }
        else
        {
            auto slider = std::make_unique<juce::Slider> (juce::Slider::LinearBar, juce::Slider::TextBoxLeft);
            slider->setRange (info.min, info.max, 0.0);
            slider->setDoubleClickReturnValue (true, info.def);
            slider->setNumDecimalPlacesToDisplay (2);
            slider->setColour (juce::Slider::trackColourId, juce::Colours::grey.withAlpha (0.5f));
            slider->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
            if (info.unit != nullptr && info.unit[0] != 0)
                slider->setTextValueSuffix (" " + juce::String (info.unit));
            addAndMakeVisible (slider.get());

            auto* rowPtr = row.get();
            slider->onDragStart = [this, rowPtr]
            {
                if (auto slot = slotRef.lock()) {
                    rowPtr->dragging = true;
                    rowPtr->valueAtDragStart = slot->getEffect().getParameter (rowPtr->parameter);
                }
            };
            slider->onValueChange = [this, rowPtr]
            {
                auto slot = slotRef.lock();
                if (slot == nullptr)
                    return;
                const auto value = static_cast<float> (rowPtr->slider->getValue());
                if (rowPtr->dragging)
                    slot->getEffect().setParameter (rowPtr->parameter, value);   // live, the step comes at the drag's end
                else
                    commit (rowPtr->parameter, slot->getEffect().getParameter (rowPtr->parameter), value);
            };
            slider->onDragEnd = [this, rowPtr]
            {
                if (! rowPtr->dragging)
                    return;
                rowPtr->dragging = false;
                if (auto slot = slotRef.lock())
                    commit (rowPtr->parameter, rowPtr->valueAtDragStart, slot->getEffect().getParameter (rowPtr->parameter));
            };
            row->slider = std::move (slider);
        }

        rows.push_back (std::move (row));
    }

    setSize (width, getPreferredHeight());
    refresh();
    startTimerHz (10);
}

EffectEditorComponent::~EffectEditorComponent()
{
    stopTimer();
}

int EffectEditorComponent::getPreferredHeight() const
{
    return headerHeight + static_cast<int> (rows.size()) * rowHeight + margin;
}

bool EffectEditorComponent::isStillValid() const
{
    return slotIndex() >= 0;
}

int EffectEditorComponent::slotIndex() const
{
    auto slot = slotRef.lock();
    auto* chain = slot != nullptr ? context.chain (address) : nullptr;
    if (chain == nullptr)
        return -1;

    const auto& slots = chain->getSlots();
    for (size_t i = 0; i < slots.size(); ++i)
        if (slots[i] == slot)
            return static_cast<int> (i);
    return -1;
}

void EffectEditorComponent::commit (int parameter, float oldValue, float newValue)
{
    auto slot = slotRef.lock();
    const auto index = slotIndex();
    if (slot == nullptr || index < 0)
        return;

    auto& effect = slot->getEffect();

    // the value as the effect will hold it (clamped), then back, so the action performs the change
    effect.setParameter (parameter, newValue);
    const auto applied = effect.getParameter (parameter);
    effect.setParameter (parameter, oldValue);
    if (juce::approximatelyEqual (applied, oldValue)) {
        refresh();
        return;
    }

    const auto& info = effect.getParameters()[static_cast<size_t> (parameter)];
    context.undoManager().beginNewTransaction();
    context.undoManager().perform (new UndoableEffectParameterAction (context.container(), address, index, parameter, oldValue, applied),
                                   "Set " + juce::String (effectName) + " " + juce::String (info.name));
}

void EffectEditorComponent::refresh()
{
    auto slot = slotRef.lock();
    if (slot == nullptr)
        return;

    auto& effect = slot->getEffect();
    bypassToggle->setToggleState (! slot->isEnabled(), juce::dontSendNotification);

    for (auto& row : rows)
    {
        if (row->dragging)
            continue;
        const auto value = effect.getParameter (row->parameter);
        if (row->slider != nullptr)
            row->slider->setValue (value, juce::dontSendNotification);
        if (row->toggle != nullptr)
            row->toggle->setToggleState (value >= 0.5f, juce::dontSendNotification);
    }
}

void EffectEditorComponent::timerCallback()
{
    if (! isStillValid()) {
        stopTimer();
        if (onSlotGone != nullptr)
            juce::MessageManager::callAsync (onSlotGone);   // the owner deletes this component: not from inside its own timer
        return;
    }
    refresh();
}

void EffectEditorComponent::resized()
{
    auto bounds = getLocalBounds().reduced (margin, 0);
    bypassToggle->setBounds (bounds.removeFromTop (headerHeight).reduced (0, 4));

    for (auto& row : rows)
    {
        auto line = bounds.removeFromTop (rowHeight).reduced (0, 3);
        row->label->setBounds (line.removeFromLeft (labelWidth));
        if (row->slider != nullptr)
            row->slider->setBounds (line);
        if (row->toggle != nullptr)
            row->toggle->setBounds (line.removeFromLeft (rowHeight));
    }
}

void EffectEditorComponent::paint (juce::Graphics& g)
{
    g.fillAll (findColour (audium::secondaryBackgroundColourId));
}
