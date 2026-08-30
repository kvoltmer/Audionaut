//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "PlayListItemDraggerControl.h"
#include "Application/AudiumCommandIDs.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Analysis/AnalysisWorker.h"


const juce::String PlayListItemDraggerControl::getLabelSuffix() const
{
    // Progress takes the BPM's spot while the analyses are still running, and
    // unlike the BPM it is not gated on selection: the footer status line is
    // easy to miss, and an unselected clip otherwise gives no hint that its
    // estimate is still being computed.
    auto suffix = getAnalysisProgressSuffix();

    if (suffix.isEmpty())
    {
        if (! isSelected())
            return {};

        suffix = getBpmSuffix();
        if (suffix.isEmpty())
            return {};
    }

    // paintLabel() draws the main label then the suffix right after it; measure
    // with its own metrics so the two cannot disagree. Only show the suffix if it
    // would still fit alongside the label in the control's current width,
    // otherwise a narrow clip would show a truncated BPM estimate.
    const auto labelFont = getLabelFont();

    auto nameWidth = juce::GlyphArrangement::getStringWidth (labelFont, getLabelString());
    auto suffixWidth = juce::GlyphArrangement::getStringWidth (labelFont, suffix);

    if (labelLeftInset + nameWidth + labelSuffixGap + suffixWidth > (float) getWidth())
        return {};

    return suffix;
}

int PlayListItemDraggerControl::getAnalysisRemainingCount() const
{
    auto resourceContainer = audiumEngine->getAudioResourceContainer();
    auto analysisWorker = (resourceContainer != nullptr) ? resourceContainer->getAnalysisWorker()
                                                         : nullptr;
    if (analysisWorker == nullptr)
        return 0;

    auto remaining = 0;
    for (const auto& resource : playListItem->getRegion()->getAudioResources())
        if (resource != nullptr)
            remaining += analysisWorker->getRemainingCount(juce::File(resource->getFullPathName()));

    return remaining;
}

juce::String PlayListItemDraggerControl::getAnalysisProgressSuffix() const
{
    const auto remaining = getAnalysisRemainingCount();
    if (remaining == 0)
        return {};

    // The footer status line's sweep animation, advanced one frame per
    // timerCallback(); the count ticks down as the clip's analyses finish.
    static constexpr const char* sweepFrames[] = { "[=   ]", "[ =  ]", "[  = ]", "[   =]",
                                                   "[  = ]", "[ =  ]" };
    constexpr auto numFrames = (int) (sizeof(sweepFrames) / sizeof(sweepFrames[0]));
    const auto* frame = sweepFrames[analysisAnimationTick % numFrames];

    return juce::String(frame) + " Analysing (" + juce::String(remaining) + ")";
}

void PlayListItemDraggerControl::timerCallback()
{
    const auto analysing = getAnalysisRemainingCount() > 0;

    if (analysing)
        ++analysisAnimationTick;
    else
        analysisAnimationTick = 0;

    // One extra repaint after the last analysis finishes clears the progress
    // suffix; the results themselves arrive via the provider's change message.
    if (analysing || wasAnalysing)
        repaint();

    wasAnalysing = analysing;
}

juce::String PlayListItemDraggerControl::getBpmSuffix() const
{
    updateAnalysisPaintCache();
    return analysisPaintCache.bpmSuffix;
}

void PlayListItemDraggerControl::paint (juce::Graphics& g)
{
    DraggerControl::paint(g);

    if (! isRecording() && matchesProjectGrid())
        paintGridMatchCheckMark(g);
}

bool PlayListItemDraggerControl::matchesProjectGrid() const
{
    updateAnalysisPaintCache();
    return analysisPaintCache.gridMatch;
}

void PlayListItemDraggerControl::updateAnalysisPaintCache() const
{
    auto region = playListItem->getRegion();
    auto audioTrack = region->getAudioTrack();
    auto analysisProvider = (audioTrack != nullptr) ? audioTrack->getAnalysisProvider() : nullptr;

    if (analysisProvider == nullptr)
    {
        analysisPaintCache = {};
        return;
    }

    auto tempoProvider = playListContainer->getTempoProvider();

    const auto generation = analysisProvider->getCache()->getGeneration();
    const auto projectTempo = (tempoProvider != nullptr) ? tempoProvider->getTempo() : 0.0;
    const auto clipStartClocks = playListItem->getAbsolutePosition(audium::clocks);
    const auto playedRegionSeconds = playListItem->getRegionData(audium::seconds);

    if (analysisPaintCache.valid
        && analysisPaintCache.generation == generation
        && analysisPaintCache.item == playListItem.get()
        && analysisPaintCache.tempo == projectTempo
        && analysisPaintCache.startClocks == clipStartClocks
        && analysisPaintCache.regionSeconds == playedRegionSeconds)
        return;

    juce::StringArray bpms;

    // Resources without a beat analysis are skipped rather than counted as a
    // mismatch, but at least one analysed resource has to match - an
    // unanalysed item shows no check mark.
    auto anyAnalysed = false;
    auto allMatch = true;
    for (const auto& resource : region->getAudioResources())
    {
        if (resource == nullptr)
            continue;

        const auto audioFile = juce::File(resource->getFullPathName());
        const auto bpm = analysisProvider->getBpm(audium::AnalysisType::BeatDegara, audioFile);

        if (bpm > 0.0f)
            bpms.add(juce::String(bpm, 1));

        const auto beats = analysisProvider->getSegments(audium::AnalysisType::BeatDegara, audioFile);

        if (bpm <= 0.0f || beats.empty())
            continue;

        anyAnalysed = true;

        if (! audium::AnalysisProvider::matchesGrid(projectTempo, bpm, beats,
                                                    clipStartClocks, playedRegionSeconds))
            allMatch = false;
    }

    analysisPaintCache.valid = true;
    analysisPaintCache.generation = generation;
    analysisPaintCache.item = playListItem.get();
    analysisPaintCache.tempo = projectTempo;
    analysisPaintCache.startClocks = clipStartClocks;
    analysisPaintCache.regionSeconds = playedRegionSeconds;
    analysisPaintCache.gridMatch = anyAnalysed && allMatch;

    if (bpms.isEmpty())
        analysisPaintCache.bpmSuffix = {};
    else if (bpms.size() == 1)
        analysisPaintCache.bpmSuffix = bpms[0] + " BPM";
    else
        analysisPaintCache.bpmSuffix = "(" + bpms.joinIntoString(", ") + ") BPM";
}

void PlayListItemDraggerControl::paintGridMatchCheckMark (juce::Graphics& g) const
{
    constexpr auto size = 8.0f;
    constexpr auto rightInset = 5.0f;

    const auto right = (float) getWidth() - rightInset;
    const auto left = right - size;

    // Skip the mark when it would sit on top of the label text - measured
    // with the label's own metrics, like getLabelSuffix() does, so zoomed-out
    // (narrow) clips drop the mark rather than overpainting the name.
    const auto labelFont = getLabelFont();
    auto textRight = labelLeftInset
        + juce::GlyphArrangement::getStringWidth (labelFont, getLabelString());

    const auto suffix = getLabelSuffix();
    if (suffix.isNotEmpty())
        textRight += labelSuffixGap
            + juce::GlyphArrangement::getStringWidth (labelFont, suffix);

    if (left < textRight + labelSuffixGap)
        return;
    const auto top = ((float) draggerHeight - size) / 2.0f;

    juce::Path checkMark;
    checkMark.startNewSubPath (left, top + size * 0.55f);
    checkMark.lineTo (left + size * 0.35f, top + size);
    checkMark.lineTo (right, top + size * 0.15f);

    g.setColour (juce::Colours::limegreen);
    g.strokePath (checkMark, juce::PathStrokeType (1.5f));
}

bool PlayListItemDraggerControl::validateData()
{
    bool result = playListItem->validateData();
    // sort by position
    playListContainer->sortByPosition();
    return result;
}


void PlayListItemDraggerControl::shiftSelect()
{
    setSelected(true, false);
    
    // create union selection rectangle
    juce::Rectangle<int> rect;
    for (auto control : regionSelector->playListItemDraggerControls) {
        if (control->isSelected()) {
            rect = rect.getUnion(control->getScreenBounds());
        }
    }
    
    // select if it contains rectange
    for (auto control : regionSelector->playListItemDraggerControls) {
        if (rect.contains(control->getScreenBounds()) &&
            !control->isSelected()) {
            control->setSelected(true, false);
        }
    }
}

static void contextMenuCallback (int result, PlayListItemDraggerControl* component)
{
    if (component != nullptr &&
        result != 0) {
        switch (result) {
            case CommandIDs::exportSelectedItemsId:
                component->exportSelectedPlayListItem();
                break;
            default:
                break;
        }
    }
}

void PlayListItemDraggerControl::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) {
        PopupMenu m;
        m.addItem (CommandIDs::exportSelectedItemsId, TRANS ("Export..."), true);
        m.setLookAndFeel (&getLookAndFeel());
        m.showMenuAsync (PopupMenu::Options().withStandardItemHeight(AudiumLookAndFeel::popupMenuItemHeight),
                         ModalCallbackFunction::forComponent (contextMenuCallback, this));
    }
    
    // call base class
    DraggerControl::mouseDown(e);
}

void PlayListItemDraggerControl::exportSelectedPlayListItem()
{
    exporter = std::make_unique<audium::PlayListItemExport>(audiumEngine,
                                                            playListItem,
                                                            true);
    exporter->exportItem();
}
