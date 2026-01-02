//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "PlayListItemDraggerControl.h"
#include "Application/AudiumCommandIDs.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"


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
