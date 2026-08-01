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
#include "Engine/Analysis/AnalysisProvider.h"


const juce::String PlayListItemDraggerControl::getLabelSuffix() const
{
    if (! isSelected())
        return {};

    auto bpmSuffix = getBpmSuffix();
    if (bpmSuffix.isEmpty())
        return {};

    // paintLabel() draws the main label then the suffix right after it, at a
    // 5px left inset using a 12pt font (see DraggerControl::paintLabel); only
    // show the suffix if it would still fit alongside the label in the
    // control's current width, otherwise a narrow clip would show a
    // truncated BPM estimate.
    constexpr float labelLeftInset = 5.0f;
    constexpr float suffixGap = 4.0f;
    juce::Font labelFont (juce::FontOptions (12.0f));

    auto nameWidth = juce::GlyphArrangement::getStringWidth (labelFont, getLabelString());
    auto suffixWidth = juce::GlyphArrangement::getStringWidth (labelFont, bpmSuffix);

    if (labelLeftInset + nameWidth + suffixGap + suffixWidth > (float) getWidth())
        return {};

    return bpmSuffix;
}

juce::String PlayListItemDraggerControl::getBpmSuffix() const
{
    auto region = playListItem->getRegion();
    auto audioTrack = region->getAudioTrack();
    if (audioTrack == nullptr)
        return {};

    auto analysisProvider = audioTrack->getAnalysisProvider();
    if (analysisProvider == nullptr)
        return {};

    juce::StringArray bpms;
    for (const auto& resource : region->getAudioResources())
    {
        if (resource == nullptr)
            continue;

        const auto audioFile = juce::File(resource->getFullPathName());

        auto bpm = analysisProvider->getBpm(audium::AnalysisType::BeatDegara, audioFile);

        if (bpm > 0.0f)
            bpms.add(juce::String(bpm, 1));
    }

    if (bpms.isEmpty())
        return {};

    if (bpms.size() == 1)
        return bpms[0] + " BPM";

    return "(" + bpms.joinIntoString(", ") + ") BPM";
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
