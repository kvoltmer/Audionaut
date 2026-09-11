//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <cmath>
#include <iostream>

#include "AutoEditOverlayControl.h"
#include "Engine/ActionMessages.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Selection/SelectionManager.h"
#include "Interface/Controls/RegionSelector.h"

#if !defined(AUDIONAUT_HEADLESS)
#include "Application/AudiumApplication.h"
#include "Interface/Dialogs/AutoEditSettingsComponent.h"
#endif

namespace {

// The measure parameter's range. Stepping halves or doubles, so every
// reachable value is a musically meaningful power of two.
constexpr double minMeasures = 1.0;
constexpr double maxMeasures = 64.0;

// Xfade: two fade ramps crossing at the joint.
juce::Path xfadeIconPath()
{
    juce::Path lines;
    lines.startNewSubPath (3.5f, 18.0f);
    lines.quadraticTo (12.0f, 18.0f, 20.5f, 6.0f);
    lines.startNewSubPath (20.5f, 18.0f);
    lines.quadraticTo (12.0f, 18.0f, 3.5f, 6.0f);

    juce::Path path;
    juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                          juce::PathStrokeType::rounded).createStrokedPath (path, lines);
    return path;
}

} // namespace

AutoEditOverlayControl::AutoEditOverlayControl(std::shared_ptr<audium::AudiumEngine> audiumEngine_,
                                               std::shared_ptr<RegionSelector> regionSelector_) :
    ClipOverlayBase(audiumEngine_, regionSelector_)
{
    // More/less speak in segments: more segments means shorter ones, so More
    // halves the measures and Less doubles them - hence minus for Less and
    // plus for More.
    lessButton = makeIconButton (TRANS ("Less"), minusIconPath());
    addAndMakeVisible (lessButton.get());
    lessButton->onClick = [this]() {
        stepMeasures(true);
    };

    moreButton = makeIconButton (TRANS ("More"), plusIconPath());
    addAndMakeVisible (moreButton.get());
    moreButton->onClick = [this]() {
        stepMeasures(false);
    };

    // The crossfade default comes from the Settings dialog's Auto Edit tab;
    // the button is the per-apply override. The accent colour marks the
    // toggled state like the header's Loop button.
    xfadeButton = makeIconButton (TRANS ("Xfade"), xfadeIconPath());
    xfadeButton->setClickingTogglesState (true);
#if !defined(AUDIONAUT_HEADLESS)
    xfadeButton->setToggleState (AutoEditSettingsComponent::readCrossfadesEnabled (AudiumApplication::getPreferences()),
                                 juce::dontSendNotification);
#else
    xfadeButton->setToggleState (true, juce::dontSendNotification);
#endif
    xfadeButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff12a4e2));
    addAndMakeVisible (xfadeButton.get());

    applyButton = makeIconButton (TRANS ("Apply"), checkIconPath());
    addAndMakeVisible (applyButton.get());
    applyButton->onClick = [this]() {
        apply();
    };

    updateStepButtons();
}

int AutoEditOverlayControl::getPreferredWidth() const
{
    return padding + buttonWidth + stepGap + buttonWidth
           + gap + buttonWidth + stepGap + buttonWidth + padding;
}

int AutoEditOverlayControl::getMinimumWidth() const
{
    return padding + buttonWidth + padding;
}

void AutoEditOverlayControl::dismissOverlay()
{
    if (auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider())
        analysisProvider->clearMergePreview();
}

void AutoEditOverlayControl::overlayShown()
{
    // The preview may have moved here from another clip; its measures
    // value travels with it.
    if (auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider())
        if (analysisProvider->getMergePreviewMeasures() > 0.0)
            measures = analysisProvider->getMergePreviewMeasures();

    updateStepButtons();

#if !defined(AUDIONAUT_HEADLESS)
    // each time the overlay comes up, the Xfade button starts from the
    // stored preference; while shown it stays a per-apply override
    xfadeButton->setToggleState (AutoEditSettingsComponent::readCrossfadesEnabled (AudiumApplication::getPreferences()),
                                 juce::dontSendNotification);
#endif
}

audium::AutoEditConfig AutoEditOverlayControl::makeConfig() const
{
    audium::AutoEditConfig config;
    config.trackId = trackId;
    config.playlistItemId = playlistItemId;
    config.segmentMeasures = measures;
    config.crossfades = xfadeButton->getToggleState();

#if !defined(AUDIONAUT_HEADLESS)
    // the crossfade length and curve come from the Settings dialog's
    // Auto Edit tab; the test build keeps the AutoEditConfig defaults
    config.crossfadeSeconds = AutoEditSettingsComponent::readCrossfadeSeconds(AudiumApplication::getPreferences());
    config.crossfadeCurve = AutoEditSettingsComponent::readCrossfadeCurve(AudiumApplication::getPreferences());
#endif

    return config;
}

void AutoEditOverlayControl::stepMeasures(bool longer)
{
    const auto stepped = juce::jlimit (minMeasures, maxMeasures,
                                       longer ? measures * 2.0 : measures / 2.0);

    // The Less button is already disabled one step before the preview would
    // go empty (see wouldCut); this guard keeps the invariant if a step
    // arrives some other way.
    if (longer && ! wouldCut (stepped))
        return;

    measures = stepped;

    std::cout << "AutoEdit measures: " << measures << std::endl;

    updateStepButtons();
    updatePreview();
}

bool AutoEditOverlayControl::wouldCut(double steppedMeasures) const
{
    audium::AutoEdit autoEdit(audiumEngine);

    auto config = makeConfig();
    config.segmentMeasures = steppedMeasures;

    // Zero means no boundary falls inside the clip; an unresolved target or
    // missing analyses report a nonzero placeholder, so only a definite
    // zero-cut step is refused.
    return autoEdit.resolveNumSegments(config) != 0;
}

void AutoEditOverlayControl::updateStepButtons()
{
    // Less doubles the measures, More halves them (see the constructor).
    // Less also stops where doubling would leave the clip uncut - previewing
    // zero cuts would hide this control and cancel the edit.
    lessButton->setEnabled (measures < maxMeasures && wouldCut (measures * 2.0));
    moreButton->setEnabled (measures > minMeasures);

    // The labels live in the base's paint() and dim with their button.
    repaint();
}

void AutoEditOverlayControl::updatePreview()
{
    audium::AutoEdit autoEdit(audiumEngine);
    auto config = makeConfig();
    autoEdit.previewAutoEdit(config);
}

void AutoEditOverlayControl::apply()
{
    juce::MessageManager::callAsync ([engine = audiumEngine, config = makeConfig()]() mutable {
        audium::AutoEdit autoEdit(engine);

        autoEdit.invokeAutoEdit(config, [](std::string error) {
#if AUDIONAUT_HEADLESS
            std::cout << "error: " << error << std::endl;
#else
            juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                        "Error",
                                                        juce::String(error));
#endif
        });

        // The pending edit is over either way; the cleared preview hides
        // every overlay control.
        if (auto analysisProvider = engine->getAudioTrackContainer()->getAnalysisProvider())
            analysisProvider->clearMergePreview();
    });
}

void AutoEditOverlayControl::resized()
{
    ClipOverlayBase::resized();

    auto r = getContentArea().reduced (padding, verticalPadding);

    // The buttons take the top strip; the strip below is where the base's
    // paint() puts their labels.
    auto buttonRow = r.removeFromTop (buttonHeight);

    applyButton->setBounds (buttonRow.removeFromRight (buttonWidth));
    buttonRow.removeFromRight (gap);

    // At the Apply-only width (see ClipOverlayBase::updatePosition)
    // everything but Apply sits out.
    const bool stepsFit = getWidth() >= getPreferredWidth() + 2 * closeButtonOverhang;
    lessButton->setVisible (stepsFit);
    moreButton->setVisible (stepsFit);
    xfadeButton->setVisible (stepsFit);

    xfadeButton->setBounds (buttonRow.removeFromRight (buttonWidth));
    buttonRow.removeFromRight (stepGap);

    lessButton->setBounds (buttonRow.removeFromLeft (buttonWidth));
    buttonRow.removeFromLeft (stepGap);
    moreButton->setBounds (buttonRow.removeFromLeft (buttonWidth));
}

void AutoEditOverlayControl::changeListenerCallback (juce::ChangeBroadcaster*)
{
    audium::AutoEdit autoEdit(audiumEngine);

    auto config = makeConfig();

    // Only an actual clip selection re-targets the pending edit; deselecting,
    // or selecting something that is no clip, leaves it where it is.
    if (! autoEdit.targetSelectedClip(config))
        return;

    if (config.trackId == trackId && config.playlistItemId == playlistItemId)
        return;

    autoEdit.previewAutoEdit(config);

    // The edit followed the clip, so the track selection follows too - the
    // channel and playlist headers keep highlighting the track being edited.
    auto audioTrackContainer = audiumEngine->getAudioTrackContainer();
    if (auto track = audioTrackContainer->getAudioTrack(config.trackId))
    {
        if (! track->isSelected())
        {
            // move the track selection over; the clip selection stays put
            for (auto& other : audioTrackContainer->getAudioTracks())
                other->setSelected(other->getId() == track->getId());

            audioTrackContainer->sendActionMessage(audium::updateAll);
        }
    }
}
