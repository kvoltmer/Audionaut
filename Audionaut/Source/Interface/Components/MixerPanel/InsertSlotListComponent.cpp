//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "InsertSlotListComponent.h"

#include "Engine/Effects/Hosting/HostedEffect.h"
#include "Engine/Effects/Hosting/PluginHost.h"
#include "Interface/ColourIds.h"
#include "Interface/Controls/LevelMeter.h"

using namespace audium;

namespace
{
    /// The marker of a plugin that is not running: a small warning dot at the row's right.
    void paintWarningMarker (juce::Graphics& g, juce::Rectangle<int> row)
    {
        const auto spot = row.removeFromRight (10).withSizeKeepingCentre (6, 6).toFloat();
        g.setColour (MeterColours::orange.withAlpha (0.9f));
        g.fillEllipse (spot);
    }

    /// Why a hosted effect is not running, empty while it runs.
    juce::String hostedProblem (const Effect& effect)
    {
        auto* hosted = dynamic_cast<const HostedEffect*> (&effect);
        if (hosted == nullptr || hosted->getStatus() == HostedEffect::Status::loaded
            || hosted->getStatus() == HostedEffect::Status::loading)   // on its way: no problem to report
            return {};
        if (hosted->getStatus() == HostedEffect::Status::incompatibleLayout)
            return TRANS ("Passing through: ") + hosted->getError();
        return TRANS ("Not loaded: ") + hosted->getError();
    }
}

//==============================================================================
/// One slot: the effect's name, click = editor, right-click = menu, drag = reorder.
class InsertSlotListComponent::SlotRow : public juce::Component,
                                         public juce::SettableTooltipClient
{
public:
    SlotRow (InsertSlotListComponent& owner_, int index_, std::shared_ptr<EffectSlot> slot_) :
        owner (owner_),
        index (index_),
        slot (std::move (slot_)),
        problem (hostedProblem (slot->getEffect()))
    {
        setRepaintsOnMouseActivity (true);
        setTooltip (problem.isNotEmpty() ? juce::String (slot->getEffect().getType().name) + ": " + problem
                                         : juce::String (slot->getEffect().getType().name));
    }

    void paint (juce::Graphics& g) override
    {
        const auto enabled = slot->isEnabled();
        auto area = getLocalBounds().toFloat().reduced (0.5f);

        g.setColour (findColour (audium::backgroundColourId).withAlpha (isMouseOver() ? 1.0f : 0.8f));
        g.fillRoundedRectangle (area, 2.0f);

        auto text = getLocalBounds().reduced (4, 0);
        if (problem.isNotEmpty()) {
            paintWarningMarker (g, getLocalBounds());
            text.removeFromRight (8);
        }
        g.setColour (findColour (audium::defaultTextColourId).withAlpha (enabled ? 1.0f : 0.4f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (juce::String (slot->getEffect().getType().name), text, juce::Justification::centredLeft, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragged = false;
        if (e.mods.isPopupMenu())
            InsertChainActions::slotMenu (owner.context, owner.address, index, owner.editorTitle (index))
                .showMenuAsync (MixerContext::menuOptions (this));
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragged || e.mods.isPopupMenu() || e.getDistanceFromDragStart() < 4)
            return;
        dragged = true;
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
            container->startDragging (dragDescription (owner.chainKey, index), this);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        // own threshold, not mouseWasClicked(): JUCE counts a one pixel wobble as a drag
        if (! dragged && ! e.mods.isPopupMenu())
            owner.context.openEditor (owner.address, slot, owner.editorTitle (index));
    }

    InsertSlotListComponent& owner;
    const int index;
    const std::shared_ptr<EffectSlot> slot;
    const juce::String problem;
    bool dragged = false;
};

//==============================================================================
/// An element kept for a plugin this machine lacks: dimmed, with the marker; right-click removes it.
class InsertSlotListComponent::MissingRow : public juce::Component,
                                            public juce::SettableTooltipClient
{
public:
    MissingRow (InsertSlotListComponent& owner_, int position_, const nlohmann::json& element) :
        owner (owner_),
        position (position_)
    {
        setRepaintsOnMouseActivity (true);
        const auto plugin = element.contains ("plugin") ? element["plugin"] : nlohmann::json::object();
        const auto field = [&plugin, &element] (const char* key, const char* fallbackKey = nullptr)
        {
            if (plugin.contains (key) && plugin[key].is_string())
                return juce::String (plugin[key].get<std::string>());
            if (fallbackKey != nullptr && element.contains (fallbackKey) && element[fallbackKey].is_string())
                return juce::String (element[fallbackKey].get<std::string>());
            return juce::String();
        };
        name = field ("name", "type");
        const auto manufacturer = field ("manufacturer");
        auto tooltip = name;
        if (manufacturer.isNotEmpty())
            tooltip += TRANS (" by ") + manufacturer;
        if (const auto format = field ("format"); format.isNotEmpty())
            tooltip += " (" + format + ")";

        // installed, but this process cannot run it (not sandbox-safe, AUv3): say so instead of "not installed"
        juce::String reason;
        if (auto host = owner.context.engine->getPluginHost())
        {
            auto listed = host->findByTypeId (field ("type", "type").toStdString());
            if (! listed)
                listed = host->findSubstitute (name, manufacturer);
            if (listed)
                reason = PluginHost::describe (host->availabilityOf (*listed));
        }
        setTooltip (reason.isNotEmpty() ? tooltip + TRANS (" is installed, but ") + reason : tooltip + TRANS (" is not installed"));
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (findColour (audium::backgroundColourId).withAlpha (isMouseOver() ? 1.0f : 0.8f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 2.0f);

        paintWarningMarker (g, getLocalBounds());
        g.setColour (findColour (audium::defaultTextColourId).withAlpha (0.4f));
        g.setFont (juce::FontOptions (10.0f, juce::Font::italic));
        g.drawText (name, getLocalBounds().reduced (4, 0).withTrimmedRight (8), juce::Justification::centredLeft, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu())
            return;
        juce::PopupMenu menu;
        menu.addItem (TRANS ("Remove"), [ctx = owner.context, chainAddress = owner.address, at = position]
        {
            InsertChainActions::removeUnresolved (ctx, chainAddress, at);
        });
        menu.showMenuAsync (MixerContext::menuOptions (this));
    }

    InsertSlotListComponent& owner;
    const int position;
    juce::String name;
};

//==============================================================================
/// An unused slot: dark; a click lists the effect types, adds the pick at the end of the chain and opens its editor.
class InsertSlotListComponent::EmptyRow : public juce::Component,
                                          public juce::SettableTooltipClient
{
public:
    explicit EmptyRow (InsertSlotListComponent& owner_) : owner (owner_)
    {
        setRepaintsOnMouseActivity (true);
        setTooltip (TRANS ("Add an insert"));
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (findColour (audium::backgroundColourId).darker (0.5f).withAlpha (isMouseOver() ? 0.9f : 1.0f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 2.0f);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        auto ctx = owner.context;
        auto chainAddress = owner.address;
        auto title = owner.editorTitle;
        InsertChainActions::typesMenu (ctx, [ctx, chainAddress, title] (const std::string& id) { InsertChainActions::addAndEdit (ctx, chainAddress, id, title); })
            .showMenuAsync (MixerContext::menuOptions (this));
    }

    InsertSlotListComponent& owner;
};

//==============================================================================
InsertSlotListComponent::InsertSlotListComponent (MixerContext context_, InsertChainAddress address_,
                                                  std::function<juce::String (int index)> editorTitle_) :
    context (std::move (context_)),
    address (address_),
    editorTitle (std::move (editorTitle_)),
    chainKey (keyFor (address_))
{
    refresh();
}

InsertSlotListComponent::~InsertSlotListComponent() = default;

juce::String InsertSlotListComponent::keyFor (const InsertChainAddress& address)
{
    return juce::String (static_cast<int> (address.kind)) + ":" + juce::String (address.id);
}

juce::var InsertSlotListComponent::dragDescription (const juce::String& chainKey, int index)
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("mixerInsert", chainKey);
    object->setProperty ("index", index);
    return juce::var (object);
}

std::vector<InsertSlotListComponent::Entry> InsertSlotListComponent::readEntries() const
{
    std::vector<Entry> entries;
    auto* chain = context.chain (address);
    if (chain == nullptr)
        return entries;

    int index = 0;
    for (const auto& slot : chain->getSlots())
        entries.push_back ({ slot, index++, -1 });

    // the missing ones back at the places they were saved in (their indices count the saved array, in rising order)
    int position = 0;
    for (const auto& unresolved : chain->getUnresolved()) {
        const auto at = juce::jlimit (0, static_cast<int> (entries.size()), unresolved.index);
        entries.insert (entries.begin() + at, { nullptr, -1, position++ });
    }
    return entries;
}

void InsertSlotListComponent::refresh()
{
    auto current = readEntries();
    if (current != shown || rows.size() != current.size())
    {
        rows.clear();
        shown = std::move (current);
        auto* chain = context.chain (address);
        for (const auto& entry : shown) {
            if (entry.slot != nullptr)
                rows.push_back (std::make_unique<SlotRow> (*this, entry.slotIndex, entry.slot));
            else
                rows.push_back (std::make_unique<MissingRow> (*this, entry.unresolvedPosition,
                                                              chain->getUnresolved()[static_cast<size_t> (entry.unresolvedPosition)].element));
            addAndMakeVisible (rows.back().get());
        }
        layoutRows();
    }

    repaint();   // a bypass flips the dimming
}

void InsertSlotListComponent::resized()
{
    layoutRows();
}

void InsertSlotListComponent::layoutRows()
{
    const auto visibleRows = juce::jmax (1, getHeight() / rowHeight);
    const auto usedRows = static_cast<int> (rows.size());
    firstVisibleRow = juce::jlimit (0, juce::jmax (0, usedRows - visibleRows), firstVisibleRow);

    // as many empty rows as the column has room for under the used slots
    const auto emptyNeeded = juce::jmax (0, visibleRows - (usedRows - firstVisibleRow));
    while (static_cast<int> (emptyRows.size()) < emptyNeeded) {
        emptyRows.push_back (std::make_unique<EmptyRow> (*this));
        addAndMakeVisible (emptyRows.back().get());
    }
    while (static_cast<int> (emptyRows.size()) > emptyNeeded)
        emptyRows.pop_back();

    auto y = -firstVisibleRow * rowHeight;
    for (auto& row : rows) {
        row->setBounds (0, y, getWidth(), rowHeight);
        row->setVisible (y >= 0 && y + rowHeight <= getHeight());
        y += rowHeight;
    }
    for (auto& row : emptyRows) {
        row->setBounds (0, y, getWidth(), rowHeight);
        y += rowHeight;
    }
}

void InsertSlotListComponent::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (static_cast<int> (rows.size()) * rowHeight <= getHeight())
        return;
    firstVisibleRow += wheel.deltaY < 0 ? 1 : -1;
    layoutRows();
}

void InsertSlotListComponent::paint (juce::Graphics& g)
{
    if (dropRow >= 0) {
        const auto y = (dropRow - firstVisibleRow) * rowHeight;
        g.setColour (juce::Colour (audium::toggledOnColour));
        g.fillRect (0, juce::jlimit (0, getHeight() - 2, y - 1), getWidth(), 2);
    }
}

bool InsertSlotListComponent::isInterestedInDragSource (const SourceDetails& details)
{
    auto* object = details.description.getDynamicObject();
    return object != nullptr && object->getProperty ("mixerInsert").toString() == chainKey;
}

int InsertSlotListComponent::dropRowFor (int y) const
{
    const auto row = firstVisibleRow + (y + rowHeight / 2) / rowHeight;
    return juce::jlimit (0, static_cast<int> (rows.size()), row);
}

int InsertSlotListComponent::slotIndexAtRow (int row) const
{
    // the slot index a drop before this row lands at: the slots above it (the missing rows take no index)
    int index = 0;
    for (int r = 0; r < row && r < static_cast<int> (shown.size()); ++r)
        if (shown[static_cast<size_t> (r)].slot != nullptr)
            ++index;
    return index;
}

void InsertSlotListComponent::itemDragMove (const SourceDetails& details)
{
    dropRow = dropRowFor (details.localPosition.y);
    repaint();
}

void InsertSlotListComponent::itemDragExit (const SourceDetails&)
{
    dropRow = -1;
    repaint();
}

void InsertSlotListComponent::itemDropped (const SourceDetails& details)
{
    const auto from = static_cast<int> (details.description.getDynamicObject()->getProperty ("index"));
    auto to = slotIndexAtRow (dropRowFor (details.localPosition.y));
    dropRow = -1;
    repaint();

    if (to > from)
        --to;   // the gap closes behind the slot
    if (to != from)
        InsertChainActions::move (context, address, from, to);
}
