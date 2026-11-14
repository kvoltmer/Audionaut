//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "EffectEditorComponent.h"

#include "Interface/ColourIds.h"
#include "Interface/Controls/LevelMeter.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

using namespace audium;

EffectEditorComponent::EffectEditorComponent (MixerContext context_, InsertChainAddress address_, std::shared_ptr<EffectSlot> slot) :
    context (std::move (context_)),
    address (address_),
    slotRef (slot)
{
    jassert (slot != nullptr);
    auto& effect = slot->getEffect();
    effectName = effect.getType().name;
    meterShown = effect.hasGainReductionMeter();

    bypassKey.setButtonText (TRANS ("Bypass"));
    bypassKey.getProperties().set (AudiumLookAndFeel::consoleButtonProperty, true);
    bypassKey.setColour (juce::TextButton::buttonColourId, juce::Colours::grey);
    bypassKey.setColour (juce::TextButton::buttonOnColourId, juce::Colour (audium::toggledOnColour));
    bypassKey.setClickingTogglesState (true);
    bypassKey.onStateChange = [this] { repaint(); };   // the key's shadow is painted here
    bypassKey.onClick = [this]
    {
        const auto index = slotIndex();
        if (index >= 0)
            InsertChainActions::setEnabled (context, address, index, ! bypassKey.getToggleState());
    };
    addAndMakeVisible (bypassKey);

    const auto parameters = effect.getParameters();
    for (int i = 0; i < static_cast<int> (parameters.size()); ++i)
    {
        const auto& info = parameters[static_cast<size_t> (i)];
        auto cell = std::make_unique<Cell>();
        cell->parameter = i;
        cell->heldWhileDragging = info.movesLatency;
        cell->name = juce::String (info.group != nullptr && info.label != nullptr ? info.label : info.name);
        auto* cellPtr = cell.get();

        if (info.boolean)
        {
            cell->key = std::make_unique<juce::TextButton> (TRANS ("On"));
            if (cell->name == cell->key->getButtonText())
                cell->name.clear();   // the key says it
            cell->key->getProperties().set (AudiumLookAndFeel::consoleButtonProperty, true);
            cell->key->setColour (juce::TextButton::buttonColourId, juce::Colours::grey);
            cell->key->setColour (juce::TextButton::buttonOnColourId, juce::Colour (audium::toggledOnColour));
            cell->key->setClickingTogglesState (true);
            cell->key->onStateChange = [this] { repaint(); };
            cell->key->onClick = [this, cellPtr]
            {
                if (auto slot = slotRef.lock())
                    commit (cellPtr->parameter, slot->getEffect().getParameter (cellPtr->parameter), cellPtr->key->getToggleState() ? 1.0f : 0.0f);
            };
            addAndMakeVisible (cell->key.get());
            cells.push_back (std::move (cell));
            continue;
        }

        auto dial = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
        juce::NormalisableRange<double> range (info.min, info.max);
        const auto unit = juce::String (info.unit);
        if (unit == "Hz" && info.min > 0.0f)
            range.setSkewForCentre (std::sqrt (static_cast<double> (info.min) * static_cast<double> (info.max)));   // an octave is an octave
        dial->setNormalisableRange (range);
        dial->setRotaryParameters (AudiumLookAndFeel::dialStartAngle, AudiumLookAndFeel::dialEndAngle, true);
        dial->setMouseDragSensitivity (160);
        dial->setDoubleClickReturnValue (true, info.def);
        // the wheel moves a dial in a burst of one-step drags: each a plan rebuild where the parameter moves the latency
        dial->setScrollWheelEnabled (! info.movesLatency);
        dial->setNumDecimalPlacesToDisplay (unit == "Hz" ? 0 : unit == "dB" ? 1 : 2);
        if (unit.isNotEmpty())
            dial->setTextValueSuffix (" " + unit);
        dial->getProperties().set (AudiumLookAndFeel::dialArcFromProperty, info.min < 0.0f && info.max > 0.0f ? range.convertTo0to1 (0.0) : 0.0);
        dial->getProperties().set (AudiumLookAndFeel::dialMarkProperty, range.convertTo0to1 (info.def));

        // an effect with its own reading of the values (a mode's name, say) shows that, and takes a typed value the same way
        if (effect.getParameterText (i, info.def).isNotEmpty())
        {
            dial->textFromValueFunction = [weak = slotRef, i] (double value)
            {
                if (auto owner = weak.lock())
                    return owner->getEffect().getParameterText (i, static_cast<float> (value));
                return juce::String (value, 2);
            };
            dial->valueFromTextFunction = [weak = slotRef, i] (const juce::String& text)
            {
                auto value = 0.0f;
                if (auto owner = weak.lock(); owner != nullptr && owner->getEffect().parseParameterText (i, text, value))
                    return static_cast<double> (value);
                return text.getDoubleValue();
            };
        }
        addAndMakeVisible (dial.get());

        dial->onDragStart = [this, cellPtr]
        {
            if (auto slot = slotRef.lock()) {
                cellPtr->dragging = true;
                cellPtr->valueAtDragStart = slot->getEffect().getParameter (cellPtr->parameter);
            }
        };
        dial->onValueChange = [this, cellPtr]
        {
            auto slot = slotRef.lock();
            if (slot == nullptr)
                return;
            const auto value = static_cast<float> (cellPtr->dial->getValue());
            showValue (*cellPtr);
            if (! cellPtr->dragging)
                commit (cellPtr->parameter, slot->getEffect().getParameter (cellPtr->parameter), value);
            else if (cellPtr->heldWhileDragging)
                return;   // the dial shows it, the effect gets it at the drag's end
            else if (auto* chain = context.chain (address))
                chain->setParameter (slotIndex(), cellPtr->parameter, value);   // live, the step comes at the drag's end
        };
        dial->onDragEnd = [this, cellPtr]
        {
            if (! cellPtr->dragging)
                return;
            cellPtr->dragging = false;
            if (slotRef.lock() != nullptr)
                commit (cellPtr->parameter, cellPtr->valueAtDragStart, static_cast<float> (cellPtr->dial->getValue()));   // what the dial shows: the effect's value live, the held one otherwise
        };
        cell->dial = std::move (dial);

        // the value under the dial; a double-click types one in
        cell->value = std::make_unique<juce::Label>();
        cell->value->setJustificationType (juce::Justification::centred);
        cell->value->setFont (juce::FontOptions (10.0f));
        cell->value->setColour (juce::Label::textColourId, findColour (audium::defaultTextColourId).withAlpha (0.7f));
        cell->value->setEditable (false, true, false);
        cell->value->onTextChange = [cellPtr]
        {
            auto& dial = *cellPtr->dial;
            dial.setValue (dial.getValueFromText (cellPtr->value->getText()), juce::sendNotificationSync);
            cellPtr->value->setText (dial.getTextFromValue (dial.getValue()), juce::dontSendNotification);
        };
        addAndMakeVisible (cell->value.get());

        cells.push_back (std::move (cell));
    }

    arrange();
    setSize (getPreferredWidth(), getPreferredHeight());
    refresh();
    startTimerHz (meterShown ? 25 : 10);   // the meter wants a smoother fall than the dials need
}

EffectEditorComponent::~EffectEditorComponent()
{
    stopTimer();
}

void EffectEditorComponent::arrange()
{
    const auto parameters = slotRef.lock()->getEffect().getParameters();
    const auto count = static_cast<int> (cells.size());
    const auto grouped = std::any_of (parameters.begin(), parameters.end(), [] (const auto& info) { return info.group != nullptr; });

    if (! grouped)
    {
        columns = count <= maximumColumns ? juce::jmax (1, count) : (count + 1) / 2;
        rows = juce::jmax (1, (count + columns - 1) / columns);
        for (int i = 0; i < count; ++i) {
            cells[static_cast<size_t> (i)]->column = i % columns;
            cells[static_cast<size_t> (i)]->row = i / columns;
        }
        return;
    }

    // a column per run of parameters in the same group; one without a group stands alone
    groups.clear();
    rows = 1;
    const char* previous = nullptr;
    int row = 0;
    for (auto& cell : cells)
    {
        const auto* group = parameters[static_cast<size_t> (cell->parameter)].group;
        if (groups.empty() || group == nullptr || previous == nullptr || std::string_view (group) != previous) {
            groups.push_back (group != nullptr ? juce::String (group) : juce::String());
            row = 0;
        }
        previous = group;
        cell->column = static_cast<int> (groups.size()) - 1;
        cell->row = row++;
        rows = juce::jmax (rows, row);
    }
    columns = juce::jmax (1, static_cast<int> (groups.size()));
}

juce::Rectangle<int> EffectEditorComponent::gridArea() const
{
    // the grid, centred in the room under the band and the group headings
    auto bounds = getLocalBounds().withTrimmedTop (headerHeight + (groups.empty() ? 0 : groupHeight)).reduced (margin);
    return bounds.withSizeKeepingCentre (columns * cellWidth, rows * cellHeight);
}

int EffectEditorComponent::getPreferredWidth() const
{
    return juce::jmax (meterShown ? minimumWidthWithMeter : minimumWidth, columns * cellWidth + 2 * margin);
}

int EffectEditorComponent::getPreferredHeight() const
{
    return headerHeight + (groups.empty() ? 0 : groupHeight) + rows * cellHeight + 2 * margin;
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

    // the value as the effect will hold it, without touching the effect: after a drag it holds the new value
    // already, and writing the old one back for the action to undo would give the audio thread a block at the
    // old value between the two stores. perform() stores the new value, a no-op where it is there already.
    const auto applied = effect.clampParameter (parameter, newValue);
    if (juce::approximatelyEqual (applied, oldValue)) {
        refresh();
        return;
    }

    const auto& info = effect.getParameters()[static_cast<size_t> (parameter)];
    context.undoManager().beginNewTransaction();
    context.undoManager().perform (new UndoableEffectParameterAction (context.container(), address, index, parameter, oldValue, applied),
                                   "Set " + juce::String (effectName) + " " + juce::String (info.name));
}

void EffectEditorComponent::showValue (Cell& cell)
{
    if (cell.value != nullptr && ! cell.value->isBeingEdited())
        cell.value->setText (cell.dial->getTextFromValue (cell.dial->getValue()), juce::dontSendNotification);
}

void EffectEditorComponent::refresh()
{
    auto slot = slotRef.lock();
    if (slot == nullptr)
        return;

    auto& effect = slot->getEffect();
    const auto bypassed = ! slot->isEnabled();
    bypassKey.setToggleState (bypassed, juce::dontSendNotification);

    for (auto& cell : cells)
    {
        for (auto* component : std::initializer_list<juce::Component*> { cell->dial.get(), cell->value.get(), cell->key.get() })
            if (component != nullptr)
                component->setAlpha (bypassed ? 0.45f : 1.0f);

        if (cell->dragging)
            continue;
        const auto value = effect.getParameter (cell->parameter);
        if (cell->dial != nullptr) {
            cell->dial->setValue (value, juce::dontSendNotification);
            showValue (*cell);   // after an effect's own reading changed too (a mode's name)
        }
        if (cell->key != nullptr)
            cell->key->setToggleState (value >= 0.5f, juce::dontSendNotification);
    }
}

void EffectEditorComponent::refreshMeter()
{
    auto slot = slotRef.lock();
    if (! meterShown || slot == nullptr)
        return;

    // the interval's peak takes the bar up at once; between peaks it falls at a steady rate
    const auto reading = slot->getEffect().takeGainReductionDb();
    const auto shown = juce::jmax (reading, meterDb - meterFallDbPerTick, 0.0f);
    if (juce::approximatelyEqual (shown, meterDb))
        return;
    meterDb = shown;
    repaint (meterArea);
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
    refreshMeter();
}

void EffectEditorComponent::resized()
{
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop (headerHeight).reduced (margin, 0);
    bypassKey.setBounds (header.removeFromLeft (56).withSizeKeepingCentre (56, 18));
    meterArea = meterShown ? header.withTrimmedLeft (margin) : juce::Rectangle<int>();

    const auto grid = gridArea();
    for (auto& cellPtr : cells)
    {
        auto& cell = *cellPtr;
        auto area = juce::Rectangle<int> (grid.getX() + cell.column * cellWidth, grid.getY() + cell.row * cellHeight,
                                          cellWidth, cellHeight);
        cell.nameArea = area.removeFromTop (nameHeight);
        auto valueArea = area.removeFromBottom (valueHeight);
        if (cell.dial != nullptr)
            cell.dial->setBounds (area);
        if (cell.value != nullptr)
            cell.value->setBounds (valueArea.reduced (4, 0));
        if (cell.key != nullptr)
            cell.key->setBounds (area.withSizeKeepingCentre (32, 16));
    }
}

void EffectEditorComponent::paint (juce::Graphics& g)
{
    g.fillAll (findColour (audium::secondaryBackgroundColourId));

    auto band = getLocalBounds().removeFromTop (headerHeight);
    g.setColour (findColour (audium::dividerColourId));
    g.fillRect (band);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRect (band.removeFromBottom (1));

    // the group headings over their columns, a faint rule between neighbouring groups
    const auto grid = gridArea();
    for (size_t column = 0; column < groups.size(); ++column)
    {
        const auto x = grid.getX() + static_cast<int> (column) * cellWidth;
        if (column > 0) {
            g.setColour (juce::Colours::black.withAlpha (0.25f));
            g.fillRect (x, grid.getY() - groupHeight + 2, 1, grid.getHeight() + groupHeight - 4);
        }
        g.setColour (findColour (audium::defaultTextColourId));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (groups[column], juce::Rectangle<int> (x, grid.getY() - groupHeight, cellWidth, groupHeight),
                    juce::Justification::centred, true);
    }

    g.setColour (findColour (audium::defaultTextColourId).withAlpha (0.75f));
    g.setFont (juce::FontOptions (10.0f));
    for (const auto& cell : cells)
        g.drawText (cell->name, cell->nameArea, juce::Justification::centredBottom, true);

    if (meterShown)
        paintMeter (g);

    AudiumLookAndFeel::drawConsoleButtonShadows (g, { &bypassKey });
    for (const auto& cell : cells)
        if (cell->key != nullptr)
            AudiumLookAndFeel::drawConsoleButtonShadows (g, { cell->key.get() });
}

void EffectEditorComponent::paintMeter (juce::Graphics& g) const
{
    auto slot = slotRef.lock();
    const auto dimmed = slot == nullptr || ! slot->isEnabled();
    const auto textColour = findColour (audium::defaultTextColourId);

    // the name at the left, as a dial's; then the bar in a recessed slot like the strips' meters, lying
    // down, the scale's ticks and figures under it
    auto band = meterArea;
    g.setColour (textColour.withAlpha (0.75f));
    g.setFont (juce::FontOptions (10.0f));
    const auto name = TRANS ("Gain Reduction");
    const auto nameWidth = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name);
    g.drawText (name, band.removeFromLeft (nameWidth), juce::Justification::centredLeft, false);
    band.removeFromLeft (8);

    constexpr int barHeight = 9;
    constexpr int endInset = 6;   // room for the end figures, which sit centred on their ticks
    band.reduce (endInset, 0);
    const auto slotArea = juce::Rectangle<int> (band.getX(), band.getY() + 6, band.getWidth(), barHeight).toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (slotArea, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (slotArea.expanded (0.5f), 2.5f, 1.0f);

    // the lit part grows from the left, the unlit remainder shows the colour faintly
    const auto inner = slotArea.reduced (1.0f);
    const auto colour = MeterColours::orange.withMultipliedAlpha (dimmed ? 0.45f : 1.0f);
    g.setColour (colour.withMultipliedAlpha (0.16f));
    g.fillRect (inner);
    const auto lit = juce::jlimit (0.0f, 1.0f, meterDb / meterRangeDb) * inner.getWidth();
    g.setColour (colour);
    g.fillRect (inner.withWidth (lit));

    // ticks and figures on a linear law; a figure only where it does not run into the one before
    g.setFont (AudiumLookAndFeel::withTabularFigures (juce::FontOptions (7.5f)));
    const auto tickTop = slotArea.getBottom() + 1.0f;
    const auto figureY = tickTop + 2.0f;
    auto lastFigureX = -100.0f;
    for (const auto mark : { 0.0f, 3.0f, 6.0f, 12.0f, 18.0f, 24.0f })
    {
        if (mark > meterRangeDb)
            continue;
        const auto tickX = inner.getX() + mark / meterRangeDb * inner.getWidth();
        g.setColour (textColour.withAlpha (mark <= 0.0f ? 0.7f : 0.35f));
        g.fillRect (tickX - 0.5f, tickTop, 1.0f, 2.0f);
        if (tickX - lastFigureX >= 14.0f) {
            g.drawText (juce::String (static_cast<int> (mark)),
                        juce::Rectangle<float> (tickX - 8.0f, figureY, 16.0f, 9.0f),
                        juce::Justification::centredTop, false);
            lastFigureX = tickX;
        }
    }
}
