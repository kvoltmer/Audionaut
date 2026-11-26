//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "HostedEffectEditorComponent.h"

#include <limits>

#include "Interface/ColourIds.h"
#include "Interface/Controls/LevelMeter.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

using namespace audium;

HostedEffectEditorComponent::HostedEffectEditorComponent (MixerContext context_, InsertChainAddress address_, std::shared_ptr<EffectSlot> slot_) :
    context (std::move (context_)),
    address (address_),
    slot (std::move (slot_)),
    hosted (dynamic_cast<HostedEffect&> (slot->getEffect()))
{
    shownStatus = hosted.getStatus();
    createEditor();
    followEditorSize();
    refresh();
    startTimerHz (10);
}

HostedEffectEditorComponent::~HostedEffectEditorComponent()
{
    stopTimer();

    // what the user did just before closing the window is an undo step too
    if (tracking && ! settling && (editPending || hosted.getPluginChangeCount() != seenPluginChanges))
        commitEdit();

    deleteEditor();   // before the slot, and with it the plugin the editor belongs to
}

void HostedEffectEditorComponent::deleteEditor()
{
    if (editor == nullptr)
        return;

    // the plugin made this its active editor: it hears that the editor goes before it does
    if (auto* instance = hosted.getInstance(); instance != nullptr && instance->getActiveEditor() == editor.get())
        instance->editorBeingDeleted (editor.get());
    editor->setComponentEffect (nullptr);
    editor.reset();
}

void HostedEffectEditorComponent::createEditor()
{
    auto* instance = hosted.getInstance();
    if (instance == nullptr)
        return;   // a placeholder: the text says why (or that the plugin is on its way)

    if (instance->hasEditor())
        editor.reset (instance->createEditorAndMakeActive());
    if (editor != nullptr && instance->getPluginDescription().pluginFormatName == "AudioUnit") {
        editor->setComponentEffect (&paintsNothing);
        editor->setOpaque (false);   // it paints nothing now: this component's background is to be painted behind it
    }
    if (editor == nullptr)
        editor = std::make_unique<juce::GenericAudioProcessorEditor> (*instance);

    addAndMakeVisible (editor.get());
    startTrackingEdits();
}

//==============================================================================
void HostedEffectEditorComponent::startTrackingEdits()
{
    baseline = hosted.getState();
    seenPluginChanges = hosted.getPluginChangeCount();
    seenHostEdits = hosted.getHostEditCount();
    editPending = false;
    settling = false;
    tracking = true;
}

void HostedEffectEditorComponent::trackEdits()
{
    if (! tracking)
        return;

    const auto now = juce::Time::getMillisecondCounter();

    // the host put a value or a state into the plugin (an undo, an agent's edit): what the plugin
    // reports next is the echo of that, not the user - once it is quiet, that state is the new start
    if (const auto hostEdits = hosted.getHostEditCount(); hostEdits != seenHostEdits) {
        seenHostEdits = hostEdits;
        settling = true;
        editPending = false;
        lastChange = now;
        waitingSince = now;
    }

    if (const auto changes = hosted.getPluginChangeCount(); changes != seenPluginChanges) {
        seenPluginChanges = changes;
        lastChange = now;
        if (! settling && ! editPending) {
            editPending = true;
            waitingSince = now;
        }
    }

    if (! settling && ! editPending)
        return;

    // One step per move: a held control and a run of changes end first. Neither wait is open-ended,
    // though - a plugin may report without pause (a meter it publishes as a parameter, a modulation of
    // its own) or never close a gesture it opened, and the user's moves would wait for ever: no undo
    // step, and a project that does not know it changed.
    const auto quietFor = now - lastChange;
    const auto moveIsOver = hosted.isGestureOpen() ? quietFor >= staleGestureMilliseconds   // held, but nothing moves: taken as let go
                                                   : quietFor >= quietMilliseconds || now - waitingSince >= longestRunMilliseconds;
    if (! moveIsOver)
        return;

    if (settling) {
        settling = false;
        baseline = hosted.getState();
    }
    else {
        commitEdit();   // no step when the state is what it was: reports alone change nothing
    }
}

void HostedEffectEditorComponent::commitEdit()
{
    editPending = false;
    seenPluginChanges = hosted.getPluginChangeCount();

    const auto index = slotIndex();
    if (index < 0)
        return;

    auto current = hosted.getState();
    if (current == baseline)
        return;   // reported, but the state is what it was

    // The plugin holds the new state already: the step records it (performing it is a no-op) so that the
    // project knows it changed - it asks to be saved - and the move can be undone.
    auto& undoManager = context.undoManager();
    undoManager.beginNewTransaction();
    undoManager.perform (new UndoableEffectStateAction (context.container(), address, index, baseline, current),
                         "Change " + hosted.getDescription().name);
    baseline = std::move (current);
    seenHostEdits = hosted.getHostEditCount();
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
    // the strip's room is on top of the editor's
    limits.minimumHeight += warningHeight();
    limits.maximumHeight = juce::jmin (limits.maximumHeight, std::numeric_limits<int>::max() / 2 - warningHeight()) + warningHeight();
    return limits;
}

int HostedEffectEditorComponent::warningHeight() const
{
    // the plugin is there, with its editor, and does not run: the strip over the editor says why
    return editor != nullptr && shownStatus == audium::HostedEffect::Status::incompatibleLayout ? warningStripHeight : 0;
}

void HostedEffectEditorComponent::followEditorSize()
{
    // this component is the editor; without one, the room for the text
    juce::ScopedValueSetter<bool> guard (following, true);
    if (editor == nullptr)
        setSize (messageWidth, messageHeight);
    else
        setSize (juce::jmax (minimumWidth, editor->getWidth()), juce::jmax (1, editor->getHeight()) + warningHeight());
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

    const auto bounds = getLocalBounds().withTrimmedTop (warningHeight());
    if (following || ! editor->isResizable()) {
        // the editor keeps its own size, centred when the window is wider than it
        editor->setTopLeftPosition ((bounds.getWidth() - editor->getWidth()) / 2, bounds.getY());
        return;
    }
    // the window was resized: its room is the editor's, within what the editor's own rules allow
    // (a fixed aspect ratio, say - the window's limits are only its smallest and largest size)
    juce::ScopedValueSetter<bool> guard (following, true);
    editor->setBoundsConstrained (bounds);
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
    if (const auto status = hosted.getStatus(); status != shownStatus) {
        shownStatus = status;
        if (editor != nullptr)
            followEditorSize();   // the strip over the editor comes or goes with a refused channel count
        repaint();   // a placeholder's text follows: loading, then the plugin - or why it did not come
    }
    trackEdits();
    refresh();
}

void HostedEffectEditorComponent::paint (juce::Graphics& g)
{
    g.fillAll (findColour (audium::secondaryBackgroundColourId));

    if (editor != nullptr)
    {
        // the plugin's editor is the window - unless the plugin does not run: then the reason stands over it
        if (auto strip = getLocalBounds().removeFromTop (warningHeight()); ! strip.isEmpty())
        {
            strip = strip.reduced (margin, 0);
            g.setColour (MeterColours::orange.withAlpha (0.9f));
            g.fillEllipse (strip.removeFromLeft (12).withSizeKeepingCentre (6, 6).toFloat());
            g.setColour (findColour (audium::defaultTextColourId));
            g.setFont (juce::FontOptions (11.0f));
            g.drawFittedText (TRANS ("Passing through: ") + hosted.getError(), strip, juce::Justification::centredLeft, 2);
        }
        return;
    }

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
