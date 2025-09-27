//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "TableRegionLabel.h"
#include "Application/AudiumCommandIDs.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

using namespace juce;

static void contextMenuCallback (int result, TableRegionLabel* component)
{
    if (component != nullptr &&
        result != 0) {
        switch (result) {
            case CommandIDs::exportSelectedItemsId:
                component->exportSelectedRegion();
                break;
            default:
                break;
        }
    }
}
    
void TableRegionLabel::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) {
        PopupMenu m;
        m.addItem (CommandIDs::exportSelectedItemsId, TRANS ("Export..."), true);
        m.setLookAndFeel (&getLookAndFeel());
        m.showMenuAsync (PopupMenu::Options().withStandardItemHeight(AudiumLookAndFeel::popupMenuItemHeight),
                         ModalCallbackFunction::forComponent (contextMenuCallback, this));
    }
    
    if (auto region = getRegion(columnId, rowNumber)) {
        
        if (!e.mods.isAnyModifierKeyDown() && !region->isSelected()) {
            audioTrackContainer->getSelectionManager()->deselectAll();
        }
        else {
            auto objects = audioTrackContainer->getSelectionManager()->getSelectedObjects();
            for (auto object : objects) {
                if (auto item = dynamic_cast<audium::AudioRegion*>(object.get())) {
                    if (item->getAudioTrack() != region->getAudioTrack())
                        object->setSelected(false);
                }
            }
        }
        
        if (e.mods.isCommandDown()) {
            // toggle selection
            region->setSelected(!region->isSelected());
            if (region->isSelected())
                region->getAudioTrack()->lastRegionSelected = rowNumber;
        }
        else if (e.mods.isShiftDown()) {
            
            auto regions = region->getAudioTrack()->getRegions();
            
            // select range
            auto lastRow = region->getAudioTrack()->lastRegionSelected;
            auto firstRow = rowNumber;
            
            
            const int numRows = static_cast<int>(region->getAudioTrack()->getRegions().size());
            firstRow = juce::jlimit (0, juce::jmax (0, numRows), firstRow);
            lastRow  = juce::jlimit (0, juce::jmax (0, numRows), lastRow);
            
            juce::SparseSet<int> selected;
            selected.addRange ({ juce::jmin (firstRow, lastRow),
                                 juce::jmax (firstRow, lastRow) + 1 });

            // deselect all regions in the track
            for (auto r : regions)
                r->setSelected(false);
            
            
            // select regions in the range
            for (auto i = 0; i < selected.size(); i++) {
                if (auto r = getRegion(columnId, selected[i])) {
                    r->setSelected(true);
                    region->getAudioTrack()->lastRegionSelected = selected[i];
                }
            }
        }
        else {
            region->setSelected(true);
            region->getAudioTrack()->lastRegionSelected = rowNumber;
        }
        
        // update
        audioTrackContainer->sendActionMessage(audium::updateSelection);
    }
}

void TableRegionLabel::update(int column, int row, bool isRowSelected)
{
    columnId = column;
    rowNumber = row;
    
    juce::String text = "n/a";

    if (auto region = getRegion(columnId, rowNumber))
    {
        text = region->getName();
        
        auto textColour = region->getAudioTrack()->getColour();
        
        if (region->isSelected()) {
            setColour (juce::Label::textColourId, textColour.brighter());
            setColour (juce::Label::backgroundColourId,
                       findColour(audium::listBoxBackgroundColourId));
        }
        else {
            setColour (juce::Label::textColourId, textColour);
            setColour (juce::Label::backgroundColourId,
                       juce::Colours::transparentBlack);
        }
    }
    setText (text, juce::dontSendNotification);
}

void TableRegionLabel::exportSelectedRegion()
{
    if (auto audioRegion = getRegion(columnId, rowNumber)) {
        exporter = std::make_unique<audium::PlayListItemExport>(audiumEngine,
                                                                audioRegion,
                                                                audioRegion->getAudioTrack()->getPlayListContainer());
    }
}
