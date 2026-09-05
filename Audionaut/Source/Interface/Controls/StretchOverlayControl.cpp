//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <cmath>

#include "StretchOverlayControl.h"
#include "Engine/ActionMessages.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Selection/ClipOverlayTarget.h"
#include "Engine/Selection/SelectionManager.h"
#include "Interface/Controls/RegionSelector.h"

namespace {

constexpr int sliderWidth = 90;

constexpr double semitoneFactor = 1.0594630943592953;   // 2^(1/12)

} // namespace

StretchOverlayControl::StretchOverlayControl(std::shared_ptr<audium::AudiumEngine> audiumEngine_,
                                             std::shared_ptr<RegionSelector> regionSelector_) :
    ClipOverlayBase(audiumEngine_, regionSelector_)
{
    // The slider is the readout and the editor in one: a LinearBar draws its
    // own value text. Log-shaped around x1 so octaves take equal travel.
    // Deliberately NOT a SliderControl - its mouseExit would hand the
    // RegionSelector back mid-session; the base owns the selector state.
    speedSlider = std::make_unique<juce::Slider>();
    speedSlider->setSliderStyle (juce::Slider::LinearBar);
    speedSlider->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    speedSlider->setColour (juce::Slider::trackColourId, juce::Colours::grey.withAlpha (0.5f));

    auto range = juce::NormalisableRange<double> (audium::PlayListItem::minSpeedRatio,
                                                  audium::PlayListItem::maxSpeedRatio);
    range.setSkewForCentre (1.0);
    speedSlider->setNormalisableRange (range);

    speedSlider->textFromValueFunction = [] (double value) {
        return juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + juce::String (value, 2);
    };
    speedSlider->valueFromTextFunction = [] (const juce::String& text) {
        return text.retainCharacters ("0123456789.-").getDoubleValue();
    };
    speedSlider->setDoubleClickReturnValue (true, 1.0);
    speedSlider->setVelocityModeParameters (1.0, 1, 0.05);
    speedSlider->setVelocityBasedMode (true);
    speedSlider->setValue (1.0, juce::dontSendNotification);
    speedSlider->onValueChange = [this] { applyRatio (speedSlider->getValue()); };
    addAndMakeVisible (speedSlider.get());

    semitoneDownButton = makeIconButton (TRANS ("Semi -"), minusIconPath());
    addAndMakeVisible (semitoneDownButton.get());
    semitoneDownButton->onClick = [this] {
        speedSlider->setValue (speedSlider->getValue() / semitoneFactor, juce::sendNotificationSync);
    };

    semitoneUpButton = makeIconButton (TRANS ("Semi +"), plusIconPath());
    addAndMakeVisible (semitoneUpButton.get());
    semitoneUpButton->onClick = [this] {
        speedSlider->setValue (speedSlider->getValue() * semitoneFactor, juce::sendNotificationSync);
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
    // the pending session still has to become its one undo step. Runs
    // before the base destructor's listener cleanup.
    commitSession();
}

int StretchOverlayControl::getPreferredWidth() const
{
    return padding + buttonWidth + stepGap + sliderWidth + stepGap
           + buttonWidth + gap + buttonWidth + padding;
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
    syncFromEngine();
    sessionDirty = false;
}

void StretchOverlayControl::overlayHidden()
{
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
    if (auto item = resolveItem())
        speedSlider->setValue (item->getSpeedRatio(), juce::dontSendNotification);
}

void StretchOverlayControl::applyRatio(double newRatio)
{
    auto item = resolveItem();

    if (item == nullptr || item->isRecording())
    {
        syncFromEngine();
        return;
    }

    // The whole overlay session is one undo step: open the container
    // snapshot on the first change, commit it when the overlay goes away.
    if (! sessionDirty)
    {
        item->onDragStart();
        sessionDirty = true;
    }

    item->setSpeedRatio (newRatio);

    // Reflect the clamp so the readout never lies.
    speedSlider->setValue (item->getSpeedRatio(), juce::dontSendNotification);

    // The relayout this triggers resizes the clip under us - follow it in
    // place instead of running the zoom fade.
    expectParentResize();
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
    if (message == audium::updateArrangementAction && ! speedSlider->isMouseButtonDown())
        syncFromEngine();
}

void StretchOverlayControl::paintLabels (juce::Graphics& g)
{
    ClipOverlayBase::paintLabels (g);

    if (speedSlider->isVisible())
        drawLabel (g, *speedSlider, TRANS ("Speed"), true);
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
    semitoneDownButton->setVisible (buttonsFit);
    semitoneUpButton->setVisible (buttonsFit);

    if (buttonsFit)
    {
        buttonRow.removeFromRight (gap);

        semitoneUpButton->setBounds (buttonRow.removeFromRight (buttonWidth));
        buttonRow.removeFromRight (stepGap);

        semitoneDownButton->setBounds (buttonRow.removeFromLeft (buttonWidth));
        buttonRow.removeFromLeft (stepGap);
    }
    else
    {
        buttonRow.removeFromRight (stepGap);
    }

    speedSlider->setBounds (buttonRow);
}

void StretchOverlayControl::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Only an actual clip selection re-targets the session; deselecting, or
    // selecting something that is no clip, leaves it where it is.
    auto container = audiumEngine->getAudioTrackContainer();

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
