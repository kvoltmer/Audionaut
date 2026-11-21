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

    /// The bypass control: a ring open at the top with a stroke through the gap.
    void paintPowerIcon (juce::Graphics& g, juce::Point<float> centre)
    {
        const auto radius = 3.5f;
        const auto gap = 0.7f;
        juce::Path icon;
        icon.addCentredArc (centre.x, centre.y + 0.5f, radius, radius, 0.0f, gap, juce::MathConstants<float>::twoPi - gap, true);
        icon.startNewSubPath (centre.x, centre.y - radius - 0.5f);
        icon.lineTo (centre.x, centre.y);
        g.strokePath (icon, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    /// The editor control: three faders lying on their sides.
    void paintFadersIcon (juce::Graphics& g, juce::Point<float> centre)
    {
        const auto halfWidth = 7.0f;
        const float knobs[] = { -2.5f, 3.0f, -1.0f };
        for (int line = 0; line < 3; ++line) {
            const auto y = centre.y + static_cast<float> (line - 1) * 3.0f;
            g.drawLine (centre.x - halfWidth, y, centre.x + halfWidth, y, 1.0f);
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ centre.x + knobs[line], y }));
        }
    }

    /// The menu control: a chevron up over a chevron down.
    void paintChevronsIcon (juce::Graphics& g, juce::Point<float> centre)
    {
        juce::Path icon;
        for (const auto direction : { -1.0f, 1.0f }) {
            icon.startNewSubPath (centre.x - 2.5f, centre.y + direction * 1.5f);
            icon.lineTo (centre.x, centre.y + direction * 4.0f);
            icon.lineTo (centre.x + 2.5f, centre.y + direction * 1.5f);
        }
        g.strokePath (icon, juce::PathStrokeType (1.2f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
    }

    /// The width of a row's outer controls: as wide as the row allows.
    int controlWidth (int rowWidth) { return juce::jmin (22, rowWidth / 3); }

    /// The menu control alone at a row's right, for a row whose plugin is not there: it has nothing to
    /// bypass and no editor, and its name stays beside the control. Returns the room left for the name.
    juce::Rectangle<int> paintMenuControl (juce::Graphics& g, juce::Rectangle<int> row, juce::Colour foreground, bool underMouse)
    {
        const auto control = row.removeFromRight (controlWidth (row.getWidth()));
        if (underMouse) {
            g.setColour (foreground.withAlpha (0.12f));
            g.fillRoundedRectangle (control.toFloat().reduced (1.0f, 1.5f), 2.0f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (control.getX() - 1, 2, 1, row.getHeight() - 4);
        g.setColour (foreground);
        paintChevronsIcon (g, control.toFloat().getCentre());
        return row;
    }
}

//==============================================================================
/// One slot: the effect's name and, under the mouse, its controls in the name's place - bypass at the left,
/// the editor's window, opened and closed, in the middle, at the right the menu that replaces it with
/// another effect type or removes it. A plugin that is not loaded has nothing to bypass and no editor: its
/// slot shows the menu alone, beside the name. Drag = reorder.
class InsertSlotListComponent::SlotRow : public juce::Component,
                                         public juce::SettableTooltipClient,
                                         private juce::Timer
{
public:
    SlotRow (InsertSlotListComponent& owner_, int index_, std::shared_ptr<EffectSlot> slot_) :
        owner (owner_),
        index (index_),
        slot (std::move (slot_))
    {
        setRepaintsOnMouseActivity (true);
        showProblem (hostedProblem (slot->getEffect()));

        // a plugin on its way arrives, or fails to, without a change of the chain: the row looks for itself
        if (isSettling())
            startTimerHz (4);
    }

    /// The marker and the tooltip follow the effect: a plugin that arrives later may fail to load, or refuse the layout.
    void refreshProblem()
    {
        if (auto current = hostedProblem (slot->getEffect()); current != problem)
            showProblem (std::move (current));
    }

    void showProblem (juce::String current)
    {
        problem = std::move (current);
        setTooltip (problem.isNotEmpty() ? juce::String (slot->getEffect().getType().name) + ": " + problem
                                         : juce::String (slot->getEffect().getType().name));
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto enabled = slot->isEnabled();
        const auto active = enabled && problem.isEmpty();   // running: lit in its strip's colour
        const auto hovered = isMouseOver (true) || isMouseButtonDown();
        auto foreground = findColour (audium::defaultTextColourId);
        const auto body = getLocalBounds().toFloat().reduced (0.5f);

        if (active)
        {
            // the background of a selected track's header: a tint of the colour on the strip's background
            const auto fill = findColour (audium::secondaryBackgroundColourId).overlaidWith (owner.colour().withAlpha (0.25f));
            g.setColour (hovered ? fill.brighter (0.15f) : fill);
            g.fillRoundedRectangle (body, 2.0f);
            foreground = fill.getPerceivedBrightness() > 0.6f ? juce::Colours::black : juce::Colours::white;
        }
        else
        {
            g.setColour (findColour (audium::backgroundColourId).withAlpha (hovered ? 1.0f : 0.8f));
            g.fillRoundedRectangle (body, 2.0f);
        }

        if (hoverZone != Zone::none && ! isNotLoaded()) {
            paintControls (g, foreground, enabled);
            return;
        }

        auto text = getLocalBounds().reduced (4, 0);
        if (hoverZone != Zone::none) {
            text = paintMenuControl (g, getLocalBounds(), foreground, hoverZone == Zone::replace).withTrimmedLeft (4);
        }
        else if (problem.isNotEmpty()) {
            paintWarningMarker (g, getLocalBounds());
            text.removeFromRight (8);
        }
        g.setColour (foreground.withAlpha (enabled ? 1.0f : 0.4f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (juce::String (slot->getEffect().getType().name), text, juce::Justification::centredLeft, true);
    }

    void mouseEnter (const juce::MouseEvent& e) override { mouseMove (e); }

    void mouseMove (const juce::MouseEvent& e) override
    {
        // a finger does not hover: it gets no controls, and its tap is a click on the name
        const auto zone = e.source.isTouch() ? Zone::none : zoneAt (e.x);
        if (zone != hoverZone) {
            hoverZone = zone;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverZone = Zone::none;
        repaint();
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        dragged = false;
        pressedZone = hoverZone;   // the control under the mouse as it is shown; none while the name shows
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
        if (dragged || e.mods.isPopupMenu())
            return;

        switch (pressedZone)
        {
            case Zone::bypass:
                InsertChainActions::setEnabled (owner.context, owner.address, index, ! slot->isEnabled());
                break;
            case Zone::replace:
                InsertChainActions::slotMenu (owner.context, owner.address, index).showMenuAsync (MixerContext::menuOptions (this));
                break;
            case Zone::edit:
            case Zone::none:
                owner.context.toggleEditor (owner.address, slot, InsertChainActions::editorTitle (owner.context, owner.address, index));
                break;
            case Zone::name:
                break;
        }
    }

private:
    enum class Zone { none, bypass, edit, replace, name };   ///< name: beside the menu of a plugin that is not loaded, no control

    /// The outer controls are as wide as the row allows, the editor's takes what is left between them.
    int sideWidth() const { return controlWidth (getWidth()); }

    /// A placeholder without an instance: nothing runs, and there is no editor to open.
    bool isNotLoaded() const
    {
        auto* hosted = dynamic_cast<const HostedEffect*> (&slot->getEffect());
        return hosted != nullptr && hosted->getStatus() == HostedEffect::Status::notLoaded;
    }

    Zone zoneAt (int x) const
    {
        if (x >= getWidth() - sideWidth())
            return Zone::replace;
        if (isNotLoaded())
            return Zone::name;
        return x < sideWidth() ? Zone::bypass : Zone::edit;
    }

    void paintControls (juce::Graphics& g, juce::Colour foreground, bool enabled)
    {
        auto area = getLocalBounds();
        const auto left = area.removeFromLeft (sideWidth());
        const auto right = area.removeFromRight (sideWidth());

        const auto zoneBounds = [&] (Zone zone) { return zone == Zone::bypass ? left : zone == Zone::replace ? right : area; };
        if (hoverZone != Zone::none) {
            g.setColour (foreground.withAlpha (0.12f));
            g.fillRoundedRectangle (zoneBounds (hoverZone).toFloat().reduced (1.0f, 1.5f), 2.0f);
        }

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (left.getRight(), 2, 1, getHeight() - 4);
        g.fillRect (right.getX() - 1, 2, 1, getHeight() - 4);

        g.setColour (foreground.withAlpha (enabled ? 1.0f : 0.4f));   // dim while bypassed, as the name is
        paintPowerIcon (g, left.toFloat().getCentre());
        g.setColour (foreground);
        paintFadersIcon (g, area.toFloat().getCentre());
        paintChevronsIcon (g, right.toFloat().getCentre());
    }

    /// On its way, or arrived and not yet asked for the chain's channel count: the planner prepares the
    /// slot once the plans that ran its placeholder are gone, and only then is it known whether it runs.
    bool isSettling() const
    {
        auto* hosted = dynamic_cast<const HostedEffect*> (&slot->getEffect());
        return hosted != nullptr && (hosted->getStatus() == HostedEffect::Status::loading || hosted->needsPrepare());
    }

    void timerCallback() override
    {
        if (isSettling())
            return;
        stopTimer();
        refreshProblem();
    }

    InsertSlotListComponent& owner;
    const int index;
    const std::shared_ptr<EffectSlot> slot;
    juce::String problem;
    Zone hoverZone = Zone::none;     ///< the control under the mouse; none while the row shows the name
    Zone pressedZone = Zone::none;   ///< the one the mouse went down on
    bool dragged = false;
};

//==============================================================================
/// An element kept for a plugin this machine lacks: dimmed, with the marker. Under the mouse it shows the
/// menu control at its right, as a slot whose plugin is not loaded does; the menu removes it.
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

        auto text = getLocalBounds().reduced (4, 0).withTrimmedRight (8);
        if (hovering)
            text = paintMenuControl (g, getLocalBounds(), findColour (audium::defaultTextColourId), overMenu).withTrimmedLeft (4);
        else
            paintWarningMarker (g, getLocalBounds());
        g.setColour (findColour (audium::defaultTextColourId).withAlpha (0.4f));
        g.setFont (juce::FontOptions (10.0f, juce::Font::italic));
        g.drawText (name, text, juce::Justification::centredLeft, true);
    }

    void mouseEnter (const juce::MouseEvent& e) override { mouseMove (e); }

    void mouseMove (const juce::MouseEvent& e) override
    {
        // a finger does not hover: it gets no control
        const auto nowHovering = ! e.source.isTouch();
        const auto nowOverMenu = nowHovering && e.x >= getWidth() - controlWidth (getWidth());
        if (nowHovering != hovering || nowOverMenu != overMenu) {
            hovering = nowHovering;
            overMenu = nowOverMenu;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hovering = overMenu = false;
        repaint();
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        pressedOnMenu = overMenu;   // the control as it is shown
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! pressedOnMenu || e.mods.isPopupMenu() || ! contains (e.getPosition()))
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
    bool hovering = false;        ///< the mouse is over the row: the menu control shows
    bool overMenu = false;        ///< ... and over that control
    bool pressedOnMenu = false;   ///< the mouse went down on it
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
        InsertChainActions::typesMenu (ctx, [ctx, chainAddress] (const std::string& id) { InsertChainActions::addAndEdit (ctx, chainAddress, id); })
            .showMenuAsync (MixerContext::menuOptions (this));
    }

    InsertSlotListComponent& owner;
};

//==============================================================================
InsertSlotListComponent::InsertSlotListComponent (MixerContext context_, InsertChainAddress address_,
                                                  std::function<juce::Colour()> colour_) :
    context (std::move (context_)),
    address (address_),
    colour (std::move (colour_)),
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
    else
    {
        for (auto& row : rows)
            if (auto* slotRow = dynamic_cast<SlotRow*> (row.get()))
                slotRow->refreshProblem();
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
