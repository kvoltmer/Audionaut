//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <cmath>

#include "StretchOverlayControl.h"
#include "Engine/ActionMessages.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Analysis/AnalysisWorker.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/ClipSpeed.h"
#include "Engine/PlayList/ClipTempo.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/PlayList/StretchMode.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Selection/ClipOverlayTarget.h"
#include "Engine/Selection/SelectionManager.h"
#include "Interface/Controls/RegionSelector.h"

namespace {

// Wide enough for "128.0 BPM" without clipping the text.
constexpr int sliderWidth = 96;
constexpr int modeBoxWidth = 96;

// ComboBox item ids (0 is reserved for "nothing selected")
constexpr int rePitchItemId = 1;
constexpr int timeStretchItemId = 2;

// The header's Loop button accent (see HeaderComponent).
const juce::Colour lockedColour (0xff12a4e2);

} // namespace

StretchOverlayControl::StretchOverlayControl(std::shared_ptr<audium::AudiumEngine> audiumEngine_,
                                             std::shared_ptr<RegionSelector> regionSelector_) :
    ClipOverlayBase(audiumEngine_, regionSelector_)
{
    // The slider is the readout and the editor in one: a LinearBar draws its
    // own value text. Deliberately NOT a SliderControl - its mouseExit would
    // hand the RegionSelector back mid-session; the base owns the selector
    // state. Its range and format follow the lock state (configureSlider).
    speedSlider = std::make_unique<juce::Slider>();
    speedSlider->setSliderStyle (juce::Slider::LinearBar);
    speedSlider->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    speedSlider->setColour (juce::Slider::trackColourId, juce::Colours::grey.withAlpha (0.5f));
    speedSlider->setVelocityModeParameters (1.0, 1, 0.05);
    speedSlider->setVelocityBasedMode (true);
    configureSlider (false);
    addAndMakeVisible (speedSlider.get());

    modeBox = std::make_unique<juce::ComboBox>();
    modeBox->addItem (TRANS ("Re-Pitch"), rePitchItemId);
    modeBox->addItem (TRANS ("Time-Stretch"), timeStretchItemId);
    modeBox->setSelectedId (rePitchItemId, juce::dontSendNotification);
    // same grey as the icon buttons (see ClipOverlayBase::makeIconButton)
    modeBox->setColour (juce::ComboBox::backgroundColourId, juce::Colours::grey);
    modeBox->setColour (juce::ComboBox::textColourId, juce::Colours::white);
    modeBox->setColour (juce::ComboBox::arrowColourId, juce::Colours::white);
    // the buttons' outline (see AudiumLookAndFeel::drawButtonBackground)
    modeBox->setColour (juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha (0.4f));
    addAndMakeVisible (modeBox.get());
    modeBox->onChange = [this] {
        applyMode (modeBox->getSelectedId() == timeStretchItemId);
    };

    lockButton = makeIconButton (TRANS ("Lock"), lockIconPath());
    lockButton->setClickingTogglesState (true);
    lockButton->setColour (juce::TextButton::buttonOnColourId, lockedColour);
    addAndMakeVisible (lockButton.get());
    lockButton->onClick = [this] { applyTempoLock (lockButton->getToggleState()); };

    // Beat trackers land an octave off now and then; these fix that in one
    // click and route through the slider like a typed value.
    halveButton = makeIconButton (juce::String (juce::CharPointer_UTF8 ("\xc3\xb7")) + "2", minusIconPath());
    addAndMakeVisible (halveButton.get());
    halveButton->onClick = [this] {
        speedSlider->setValue (speedSlider->getValue() * 0.5, juce::sendNotificationSync);
    };

    doubleButton = makeIconButton (juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + "2", plusIconPath());
    addAndMakeVisible (doubleButton.get());
    doubleButton->onClick = [this] {
        speedSlider->setValue (speedSlider->getValue() * 2.0, juce::sendNotificationSync);
    };

    applyButton = makeIconButton (TRANS ("Apply"), checkIconPath());
    addAndMakeVisible (applyButton.get());
    applyButton->onClick = [this] {
        // commit first: the cleared target hides the overlay, whose hidden
        // hook then finds a clean session and leaves the transaction alone
        commitSession();
        audiumEngine->getAudioTrackContainer()->getClipOverlayTarget()->clear();
    };
}

StretchOverlayControl::~StretchOverlayControl()
{
    // A rebuild can destroy a visible control without a visibilityChanged;
    // the pending session still has to become its one undo step, and the
    // analysis listener must not dangle. Runs before the base destructor's
    // listener cleanup.
    commitSession();

    if (auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider())
        analysisProvider->removeChangeListener (this);
}

void StretchOverlayControl::configureSlider (bool locked)
{
    sliderShowsTempo = locked;

    if (locked)
    {
        // The clip's own tempo in BPM, log-shaped around the project tempo
        // so the common small corrections get the travel.
        const auto projectTempo = audiumEngine->getPlayListScheduler()->getTempoProvider()->getTempo();
        auto range = juce::NormalisableRange<double> (audium::ClipSpeed::minClipTempo,
                                                      audium::ClipSpeed::maxClipTempo, 0.1);
        range.setSkewForCentre (juce::jlimit (audium::ClipSpeed::minClipTempo + 1.0,
                                              audium::ClipSpeed::maxClipTempo - 1.0, projectTempo));
        speedSlider->setNormalisableRange (range);

        speedSlider->textFromValueFunction = [] (double value) {
            return juce::String (value, 1) + " BPM";
        };
        speedSlider->valueFromTextFunction = [] (const juce::String& text) {
            return text.retainCharacters ("0123456789.").getDoubleValue();
        };

        // Double-click: back to the detected tempo (the project tempo when
        // the source has no beat analysis).
        auto resetTempo = projectTempo;
        if (auto item = resolveItem())
            if (auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider())
                if (auto detected = audium::ClipTempo::detectedClipTempo (*item, *analysisProvider); detected > 0.0f)
                    resetTempo = detected;
        speedSlider->setDoubleClickReturnValue (true, resetTempo);

        speedSlider->onValueChange = [this] { applyClipTempo (speedSlider->getValue()); };
    }
    else
    {
        // Log-shaped around x1 so octaves take equal travel.
        auto range = juce::NormalisableRange<double> (audium::ClipSpeed::minSpeedRatio,
                                                      audium::ClipSpeed::maxSpeedRatio);
        range.setSkewForCentre (1.0);
        speedSlider->setNormalisableRange (range);

        speedSlider->textFromValueFunction = [] (double value) {
            return juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + juce::String (value, 2);
        };
        speedSlider->valueFromTextFunction = [] (const juce::String& text) {
            return text.retainCharacters ("0123456789.-").getDoubleValue();
        };
        speedSlider->setDoubleClickReturnValue (true, 1.0);
        speedSlider->onValueChange = [this] { applyRatio (speedSlider->getValue()); };
    }
}

int StretchOverlayControl::getPreferredWidth() const
{
    // The /2 x2 slots are reserved in both lock states so toggling the lock
    // does not move the panel; unlocked, the slider takes their room.
    return padding + modeBoxWidth + gap + buttonWidth + stepGap
           + buttonWidth + stepGap + sliderWidth + stepGap + buttonWidth
           + gap + buttonWidth + padding;
}

int StretchOverlayControl::getMinimumWidth() const
{
    return padding + sliderWidth + stepGap + buttonWidth + padding;
}

void StretchOverlayControl::dismissOverlay()
{
    // Escape / the close chip mean "never mind": roll the session back
    // before the cleared target hides the control.
    cancelSession();
    audiumEngine->getAudioTrackContainer()->getClipOverlayTarget()->clear();
}

void StretchOverlayControl::overlayShown()
{
    sessionDirty = false;
    syncFromEngine();

    // A beat analysis may have finished while this clip's overlay was away
    // (or may still be running - then the listener catches it).
    adoptDetectedTempo();

    if (auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider())
        analysisProvider->addChangeListener (this);
}

void StretchOverlayControl::overlayHidden()
{
    if (auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider())
        analysisProvider->removeChangeListener (this);

    commitSession();
}

std::shared_ptr<audium::PlayListItem> StretchOverlayControl::resolveItem() const
{
    auto track = audiumEngine->getAudioTrackContainer()->getAudioTrack(trackId);

    if (track == nullptr)
        return nullptr;

    return track->getPlayListContainer()->getPlayListItem(playlistItemId);
}

void StretchOverlayControl::syncFromEngine()
{
    auto item = resolveItem();
    if (item == nullptr)
        return;

    const auto locked = item->isTempoLocked();

    if (locked != sliderShowsTempo)
        configureSlider (locked);

    lockButton->setToggleState (locked, juce::dontSendNotification);
    speedSlider->setValue (locked ? item->getClipTempo() : item->getSpeedRatio(),
                           juce::dontSendNotification);
    modeBox->setSelectedId (item->getStretchMode() == audium::StretchMode::Stretch
                                ? timeStretchItemId : rePitchItemId,
                            juce::dontSendNotification);

    // the /2 x2 pair only exists for a tempo
    resized();
    repaint();
}

void StretchOverlayControl::applyRatio (double newRatio)
{
    auto item = resolveItem();

    if (item == nullptr || item->isRecording() || item->isTempoLocked())
    {
        syncFromEngine();
        return;
    }

    ensureSessionOpen (*item);

    item->setSpeedRatio (newRatio);

    // Reflect the clamp so the readout never lies.
    speedSlider->setValue (item->getSpeedRatio(), juce::dontSendNotification);

    // The relayout this triggers resizes the clip under us - follow it in
    // place instead of running the zoom fade.
    expectParentResize();
    audiumEngine->getAudioTrackContainer()->sendActionMessage (audium::updateArrangementAction);
}

void StretchOverlayControl::applyClipTempo (double bpm)
{
    auto item = resolveItem();

    if (item == nullptr || item->isRecording() || ! item->isTempoLocked())
    {
        syncFromEngine();
        return;
    }

    ensureSessionOpen (*item);

    item->setClipTempo (bpm);
    speedSlider->setValue (item->getClipTempo(), juce::dontSendNotification);

    expectParentResize();
    audiumEngine->getAudioTrackContainer()->sendActionMessage (audium::updateArrangementAction);
}

void StretchOverlayControl::applyTempoLock (bool shouldLock)
{
    auto item = resolveItem();

    if (item == nullptr || item->isRecording())
    {
        syncFromEngine();
        return;
    }

    ensureSessionOpen (*item);

    if (shouldLock)
    {
        auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider();
        const auto tempoKnown = analysisProvider != nullptr
                                    ? audium::ClipTempo::lockToTempo (*item, *analysisProvider)
                                    : (item->setTempoLocked (true), item->getClipTempo() > 0.0);

        // No beat analysis yet: queue it, the listener adopts the result.
        // Until then the clip plays as recorded (ratio 1.0), locked.
        if (! tempoKnown)
            if (auto worker = audiumEngine->getAudioResourceContainer()->getAnalysisWorker())
                audium::ClipTempo::requestClipTempoDetection (*item, *worker);
    }
    else
    {
        item->setTempoLocked (false);
    }

    syncFromEngine();

    // locking changes the clip's length (unlike the Mode box)
    expectParentResize();
    audiumEngine->getAudioTrackContainer()->sendActionMessage (audium::updateArrangementAction);
}

void StretchOverlayControl::adoptDetectedTempo()
{
    auto item = resolveItem();
    if (item == nullptr || ! item->isTempoLocked() || item->getClipTempo() > 0.0)
        return;

    auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider();
    if (analysisProvider == nullptr)
        return;

    if (auto detected = audium::ClipTempo::detectedClipTempo (*item, *analysisProvider); detected > 0.0f)
    {
        applyClipTempo (detected);

        // the double-click reset learns the detected value too
        configureSlider (true);
        speedSlider->setValue (item->getClipTempo(), juce::dontSendNotification);
    }
}

void StretchOverlayControl::ensureSessionOpen (audium::PlayListItem& item)
{
    // The whole overlay session is one undo step: open the container
    // snapshot on the first change, commit it when the overlay goes away.
    if (! sessionDirty)
    {
        item.onDragStart();
        sessionDirty = true;
    }
}

void StretchOverlayControl::applyMode (bool pitchPreserving)
{
    auto item = resolveItem();

    if (item == nullptr || item->isRecording())
    {
        syncFromEngine();
        return;
    }

    ensureSessionOpen (*item);

    item->setStretchMode (pitchPreserving ? audium::StretchMode::Stretch
                                          : audium::StretchMode::RePitch);

    // no relayout (the length is mode-independent); the broadcast keeps
    // the arrangement's readouts honest, playback switches over on Apply
    audiumEngine->getAudioTrackContainer()->sendActionMessage (audium::updateArrangementAction);
}

void StretchOverlayControl::commitSession()
{
    if (! sessionDirty)
        return;

    sessionDirty = false;

    // If the item is gone (deleted, project closed) the open action died
    // with it; there is nothing coherent left to commit.
    if (auto item = resolveItem())
        item->onDragEnd (TRANS ("Stretch Clip"));
}

void StretchOverlayControl::cancelSession()
{
    if (! sessionDirty)
        return;

    sessionDirty = false;

    // The rollback re-reads the container from the opening snapshot, which
    // broadcasts its own updateAll - the arrangement redraws by itself.
    if (auto item = resolveItem())
        item->onDragCancel();
}

void StretchOverlayControl::overlayActionReceived (const juce::String& message)
{
    if ((message == audium::updateArrangementAction || message == audium::tempoChanged)
        && ! speedSlider->isMouseButtonDown())
        syncFromEngine();
}

void StretchOverlayControl::paintLabels (juce::Graphics& g)
{
    ClipOverlayBase::paintLabels (g);

    if (speedSlider->isVisible())
        drawLabel (g, *speedSlider, sliderShowsTempo ? TRANS ("Tempo") : TRANS ("Speed"), true);

    if (modeBox->isVisible())
        drawLabel (g, *modeBox, TRANS ("Mode"), true);
}

void StretchOverlayControl::resized()
{
    ClipOverlayBase::resized();

    auto r = getContentArea().reduced (padding, verticalPadding);
    auto buttonRow = r.removeFromTop (buttonHeight);

    // Apply must survive every width tier - the dismiss gestures cancel,
    // so a panel without Apply could not confirm. At the minimum width
    // (see ClipOverlayBase::updatePosition) only slider + Apply remain.
    applyButton->setBounds (buttonRow.removeFromRight (buttonWidth));

    const bool buttonsFit = getWidth() >= getPreferredWidth() + 2 * closeButtonOverhang;
    const bool tempoButtonsShown = buttonsFit && sliderShowsTempo;
    modeBox->setVisible (buttonsFit);
    lockButton->setVisible (buttonsFit);
    halveButton->setVisible (tempoButtonsShown);
    doubleButton->setVisible (tempoButtonsShown);

    if (buttonsFit)
    {
        modeBox->setBounds (buttonRow.removeFromLeft (modeBoxWidth));
        buttonRow.removeFromLeft (gap);

        lockButton->setBounds (buttonRow.removeFromLeft (buttonWidth));
        buttonRow.removeFromLeft (stepGap);

        buttonRow.removeFromRight (gap);

        if (tempoButtonsShown)
        {
            halveButton->setBounds (buttonRow.removeFromLeft (buttonWidth));
            buttonRow.removeFromLeft (stepGap);

            doubleButton->setBounds (buttonRow.removeFromRight (buttonWidth));
            buttonRow.removeFromRight (stepGap);
        }
    }
    else
    {
        buttonRow.removeFromRight (stepGap);
    }

    speedSlider->setBounds (buttonRow);
}

void StretchOverlayControl::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    auto container = audiumEngine->getAudioTrackContainer();

    // A finished analysis: only interesting for a locked clip still
    // waiting for its tempo.
    if (source == container->getAnalysisProvider().get())
    {
        adoptDetectedTempo();
        return;
    }

    // Only an actual clip selection re-targets the session; deselecting, or
    // selecting something that is no clip, leaves it where it is.
    for (const auto& object : container->getSelectionManager()->getSelectedObjects())
    {
        if (auto* item = dynamic_cast<audium::PlayListItem*>(object.get()))
        {
            const auto newTrackId = item->getRegion()->getAudioTrack()->getId();
            const auto newItemId = item->getId();

            if (newTrackId == trackId && newItemId == playlistItemId)
                return;

            // The broadcast hides this control (committing its session) and
            // shows the new clip's overlay fresh.
            container->getClipOverlayTarget()->set(newTrackId, newItemId);
            return;
        }
    }
}
