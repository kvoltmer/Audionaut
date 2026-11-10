//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "InsertSlotListComponent.h"

#include "Interface/ColourIds.h"

using namespace audium;

//==============================================================================
/// One slot: the effect's name, click = editor, right-click = menu, drag = reorder.
class InsertSlotListComponent::SlotRow : public juce::Component,
                                         public juce::SettableTooltipClient
{
public:
    SlotRow (InsertSlotListComponent& owner_, int index_, std::shared_ptr<EffectSlot> slot_) :
        owner (owner_),
        index (index_),
        slot (std::move (slot_))
    {
        setRepaintsOnMouseActivity (true);
        setTooltip (juce::String (slot->getEffect().getType().name));
    }

    void paint (juce::Graphics& g) override
    {
        const auto enabled = slot->isEnabled();
        auto area = getLocalBounds().toFloat().reduced (0.5f);

        g.setColour (findColour (audium::backgroundColourId).withAlpha (isMouseOver() ? 1.0f : 0.8f));
        g.fillRoundedRectangle (area, 2.0f);

        g.setColour (findColour (audium::defaultTextColourId).withAlpha (enabled ? 1.0f : 0.4f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (juce::String (slot->getEffect().getType().name), getLocalBounds().reduced (4, 0),
                    juce::Justification::centredLeft, true);
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
    bool dragged = false;
};

//==============================================================================
/// An unused slot: dark; a click lists the effect types and adds the pick at the end of the chain.
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
        auto context = owner.context;
        auto address = owner.address;
        InsertChainActions::typesMenu (context, [context, address] (const std::string& id) { InsertChainActions::add (context, address, id); })
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
    return juce::String (static_cast<int> (address.kind)) + ":" + juce::String (address.trackId) + ":" + juce::String (address.channelNumber);
}

juce::var InsertSlotListComponent::dragDescription (const juce::String& chainKey, int index)
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("mixerInsert", chainKey);
    object->setProperty ("index", index);
    return juce::var (object);
}

void InsertSlotListComponent::refresh()
{
    auto* chain = context.chain (address);

    std::vector<EffectSlot*> current;
    if (chain != nullptr)
        for (const auto& slot : chain->getSlots())
            current.push_back (slot.get());

    if (current != shownSlots || slotRows.size() != current.size())
    {
        slotRows.clear();
        shownSlots = current;
        if (chain != nullptr) {
            int index = 0;
            for (const auto& slot : chain->getSlots()) {
                slotRows.push_back (std::make_unique<SlotRow> (*this, index++, slot));
                addAndMakeVisible (slotRows.back().get());
            }
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
    const auto usedRows = static_cast<int> (slotRows.size());
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
    for (auto& row : slotRows) {
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
    if (static_cast<int> (slotRows.size()) * rowHeight <= getHeight())
        return;
    firstVisibleRow += wheel.deltaY < 0 ? 1 : -1;
    layoutRows();
}

void InsertSlotListComponent::paint (juce::Graphics& g)
{
    if (dropIndex >= 0) {
        const auto y = (dropIndex - firstVisibleRow) * rowHeight;
        g.setColour (juce::Colour (audium::toggledOnColour));
        g.fillRect (0, juce::jlimit (0, getHeight() - 2, y - 1), getWidth(), 2);
    }
}

bool InsertSlotListComponent::isInterestedInDragSource (const SourceDetails& details)
{
    auto* object = details.description.getDynamicObject();
    return object != nullptr && object->getProperty ("mixerInsert").toString() == chainKey;
}

int InsertSlotListComponent::dropIndexFor (int y) const
{
    const auto row = firstVisibleRow + (y + rowHeight / 2) / rowHeight;
    return juce::jlimit (0, static_cast<int> (slotRows.size()), row);
}

void InsertSlotListComponent::itemDragMove (const SourceDetails& details)
{
    dropIndex = dropIndexFor (details.localPosition.y);
    repaint();
}

void InsertSlotListComponent::itemDragExit (const SourceDetails&)
{
    dropIndex = -1;
    repaint();
}

void InsertSlotListComponent::itemDropped (const SourceDetails& details)
{
    const auto from = static_cast<int> (details.description.getDynamicObject()->getProperty ("index"));
    auto to = dropIndexFor (details.localPosition.y);
    dropIndex = -1;
    repaint();

    if (to > from)
        --to;   // the gap closes behind the slot
    if (to != from)
        InsertChainActions::move (context, address, from, to);
}
