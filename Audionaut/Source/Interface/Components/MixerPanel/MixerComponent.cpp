//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "MixerComponent.h"

#include "Interface/ColourIds.h"
#include "Interface/Components/MixerPanel/EffectEditorComponent.h"
#ifndef AUDIONAUT_HEADLESS
 #include "Interface/Dialogs/FloatingToolWindow.h"
 #include "Application/AudiumApplication.h"
 #include "Util/Preferences.h"
#endif

using namespace audium;
using namespace MixerLayout;

//==============================================================================
/// The scrolling row of track groups.
class MixerComponent::Content : public juce::Component
{
public:
    void rebuild (const MixerContext& context)
    {
        groups.clear();
        for (const auto& track : context.container().getAudioTracks()) {
            groups.push_back (std::make_unique<MixerTrackGroupComponent> (context, track));
            addAndMakeVisible (groups.back().get());
        }
        layOut();
    }

    void refresh()
    {
        for (auto& group : groups)
            group->refresh();
        layOut();
    }

    void refreshLive()
    {
        for (auto& group : groups)
            group->refreshLive();
    }

    void setSections (int sections)
    {
        for (auto& group : groups)
            group->setSections (sections);
    }

    /// Lays the groups out left to right and sizes itself to them.
    void layOut()
    {
        int x = groupGap;
        for (auto& group : groups) {
            group->setBounds (x, 0, group->getPreferredWidth(), getHeight());
            x += group->getPreferredWidth() + groupGap;
        }
        setSize (x, getHeight());
    }

    void resized() override { layOut(); }

private:
    std::vector<std::unique_ptr<MixerTrackGroupComponent>> groups;
};

//==============================================================================
MixerComponent::MixerComponent (std::shared_ptr<AudiumEngine> engine)
{
    context.engine = std::move (engine);
    context.openEditor = [safeThis = juce::Component::SafePointer<MixerComponent> (this)]
                         (const InsertChainAddress& address, std::shared_ptr<EffectSlot> slot, const juce::String& title)
    {
        if (safeThis != nullptr)
            safeThis->openEditor (address, slot, title);
    };

    content = std::make_unique<Content>();
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (false, true, false, true);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    readSectionPreference();
    sectionBar.onChange = [this] (int sections)
    {
        content->setSections (sections);
        writeSectionPreference();
        preferredHeightMayHaveChanged();
        resized();   // the fader moved with the sections, and with it the bar's toggles
    };
    addAndMakeVisible (sectionBar);
    lastPreferredHeight = getPreferredHeight();
}

int MixerComponent::getPreferredHeight() const
{
    return mixerHeight (sectionBar.getSections(), context.container().getAuxBuses().size());
}

void MixerComponent::preferredHeightMayHaveChanged()
{
    const auto height = getPreferredHeight();
    if (height == lastPreferredHeight)
        return;
    lastPreferredHeight = height;
    juce::NullCheckedInvocation::invoke (onPreferredHeightChanged);
}

void MixerComponent::readSectionPreference()
{
    int sections = allSections;
   #ifndef AUDIONAUT_HEADLESS   // the headless builds (tests, CLI) have no app preferences
    sections = juce::String (AudiumApplication::getPreferences().getValue (PreferenceKeys::mixerSections, std::to_string (allSections))).getIntValue();
   #endif
    sectionBar.setSections (sections & allSections);
}

void MixerComponent::writeSectionPreference()
{
   #ifndef AUDIONAUT_HEADLESS
    AudiumApplication::getPreferences().setValue (PreferenceKeys::mixerSections, std::to_string (sectionBar.getSections()));
   #endif
}

MixerComponent::~MixerComponent()
{
    stopTimer();
    editorWindows.clear();
}

juce::String MixerComponent::structureSignature() const
{
    // everything a strip is built from: a change rebuilds the groups, anything else is a refresh
    juce::String signature;
    const auto appendChain = [&signature] (const InsertChain& chain)
    {
        for (const auto& slot : chain.getSlots())
            signature << juce::String::toHexString (reinterpret_cast<juce::pointer_sized_int> (slot.get())) << ",";
        signature << ";";
    };

    auto& container = context.container();
    for (const auto& track : container.getAudioTracks()) {
        signature << "t" << track->getId() << ":";
        appendChain (track->getTrackInserts());
        const auto numChannels = static_cast<int> (track->audioChannelContainer->size());
        for (int c = 0; c < numChannels; ++c) {
            auto channel = track->getChannel (c);
            if (channel == nullptr)
                continue;
            signature << "c" << c << ",";   // the sends are knobs per aux: their coming and going is a refresh
        }
        signature << "|";
    }
    for (const auto& bus : container.getAuxBuses().getBuses())
        signature << "a" << bus->getId() << ",";
    return signature;
}

void MixerComponent::updateUI()
{
    const auto structure = structureSignature();
    if (structure != shownStructure) {
        shownStructure = structure;
        content->rebuild (context);
        content->setSections (sectionBar.getSections());
    }
    else {
        content->refresh();
    }

    closeEditorsWhoseSlotIsGone();
    preferredHeightMayHaveChanged();
}

void MixerComponent::resized()
{
    auto bounds = getLocalBounds();
    sectionBar.setBounds (bounds.removeFromLeft (MixerSectionBar::width));
    viewport.setBounds (bounds);
    const auto sections = sectionBar.getSections();
    const auto numAuxBuses = static_cast<int> (context.container().getAuxBuses().size());
    content->setSize (juce::jmax (content->getWidth(), viewport.getMaximumVisibleWidth()), groupHeight (sections, numAuxBuses));
    sectionBar.setFaderRange (faderRange (sections, numAuxBuses));   // the content's top is the bar's top: the viewport scrolls only sideways
}

void MixerComponent::paint (juce::Graphics& g)
{
    g.fillAll (findColour (audium::secondaryBackgroundColourId));
}

void MixerComponent::visibilityChanged()
{
    if (isVisible())
        startTimerHz (AudiumLookAndFeel::timerHz);
    else
        stopTimer();
}

void MixerComponent::timerCallback()
{
    if (isShowing())
        content->refreshLive();
}

//==============================================================================
void MixerComponent::openEditor (const InsertChainAddress& address, std::shared_ptr<EffectSlot> slot, const juce::String& title)
{
    if (slot == nullptr)
        return;

   #ifdef AUDIONAUT_HEADLESS
    juce::ignoreUnused (address, title);   // no windows without the app shell (tests, CLI)
   #else
    auto& holder = editorWindows[slot.get()];
    if (holder != nullptr) {
        holder->toFront (true);
        return;
    }

    auto* editor = new EffectEditorComponent (context, address, slot);
    editor->onSlotGone = [safeThis = juce::Component::SafePointer<MixerComponent> (this), key = slot.get()]
    {
        if (safeThis != nullptr)
            safeThis->editorWindows.erase (key);
    };

    const auto width = EffectEditorComponent::width;
    const auto height = editor->getPreferredHeight();
    const auto titleBarAllowance = 64;   // the window's limits count the native title bar, the content wants its full height
    auto* window = new FloatingToolWindow (title.toStdString(), {}, editor, holder, false,
                                           width, height, width, height, width, height + titleBarAllowance);
    window->setContentComponentSize (width, height);
    window->addKeyListener (AudiumApplication::getCommandManager().getKeyMappings());   // undo and the other shortcuts work with the editor in front
   #endif
}

void MixerComponent::closeEditorsWhoseSlotIsGone()
{
    for (auto it = editorWindows.begin(); it != editorWindows.end();)
    {
        auto* window = dynamic_cast<juce::DialogWindow*> (it->second.get());
        auto* editor = window != nullptr ? dynamic_cast<EffectEditorComponent*> (window->getContentComponent()) : nullptr;
        if (it->second == nullptr || editor == nullptr || ! editor->isStillValid())
            it = editorWindows.erase (it);
        else
            ++it;
    }
}
