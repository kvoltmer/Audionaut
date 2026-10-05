//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudioTrackBaseComponent.h"


#include "Util/EngineAccess.h"
#include "Interface/Controls/AudioTrackListBox.h"
#include "Interface/Components/MiddlePanel/ArrangementView/PlayListItemComponent.h"
#include "Interface/ColourIds.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Interface/Handlers/SnapToGridHandler.h"
#include "Interface/Dialogs/ImportOptionsComponent.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

AudioTrackBaseComponent::AudioTrackBaseComponent (std::shared_ptr<audium::AudioTrack> track,
                                        std::shared_ptr<audium::AudiumEngine> audiumEngine,
                                        std::shared_ptr<ZoomHandler> zoomHandler,
                                        std::shared_ptr<RegionSelector> regionSelector) :
    audioTrack(track),
    audiumEngine(audiumEngine),
    zoomHandler(zoomHandler),
    regionSelector(regionSelector)
{
}

void AudioTrackBaseComponent::paint (juce::Graphics& g)
{
    
    if (externalDragAndDrop) {
        auto colour = findColour(audium::secondaryBackgroundColourId).brighter();
        g.fillAll (colour.withAlpha(0.5f));
    }
    
    if (audioTrack->isSelected()) {
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 3.0f, 1.0f);
    }
    
}

void AudioTrackBaseComponent::filesDropped (const StringArray& filenames, double position, bool undo)
{
    if ( !filenames.isEmpty() ) {
        
        zoomHandler->snapToGrid(position);
        
        std::function<void (std::string)> callback = [](std::string error) {
            juce::NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                        "Failed to open File.",
                                                        "Failed to open: " + juce::String(error));
        };
        
        audioTrack->addAudioFiles(filenames, position, callback, undo);
 
    }
    
    externalDragAndDrop = false;
    regionSelector->setEnabled(true);
    zoomHandler->getSnapToGridHandler()->clearRange();
    repaint();
}

void AudioTrackBaseComponent::showContextMenu (int x)
{
    // files land where the click was, snapped like a drop
    auto position = zoomHandler->xToClocks (x);
    zoomHandler->snapToGrid (position);

    PopupMenu m;
    m.setLookAndFeel (&getLookAndFeel());
    m.addItem (TRANS ("Import Audio..."), [safeThis = Component::SafePointer<AudioTrackBaseComponent> (this), position]
    {
        if (safeThis == nullptr)
            return;
        if (auto* importer = dynamic_cast<AudioImporter*> (juce::JUCEApplication::getInstance()))
            importer->askUserToImportAudio (safeThis->audioTrack, position);
    });
    m.showMenuAsync (PopupMenu::Options().withStandardItemHeight (AudiumLookAndFeel::popupMenuItemHeight));
}

void AudioTrackBaseComponent::filesDropped (const StringArray& filenames, int x, int y)
{
    auto position = zoomHandler->xToClocks(x);
    filesDropped(filenames, position, true);
}

void AudioTrackBaseComponent::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    externalDragAndDrop = true;
    regionSelector->setEnabled(false);
    repaint();
}

void AudioTrackBaseComponent::fileDragMove (const StringArray& files, int x, int y)
{
    auto start = zoomHandler->xToClocks(x);
    auto end = start + 0.01;
    Range<double> rangeInClocks(start, end);
    
    zoomHandler->getSnapToGridHandler()->publishRange(rangeInClocks);
}

void AudioTrackBaseComponent::fileDragExit (const juce::StringArray& files)
{
    externalDragAndDrop = false;
    regionSelector->setEnabled(true);
    zoomHandler->getSnapToGridHandler()->clearRange();
    repaint();
}


void AudioTrackBaseComponent::mouseDown (const MouseEvent& e)
{
    // clips bring their own menu, and a region selection its loop menu
    const auto onSelection = regionSelector != nullptr && regionSelector->isShowing()
                             && regionSelector->getScreenBounds().contains (e.getScreenPosition());
    if (e.mods.isPopupMenu() && ! onSelection)
        showContextMenu (e.x);

    bool isSelected = audioTrack->isSelected();
    
    if (!e.mods.isAnyModifierKeyDown() && !isSelected) {
        audioTrack->getSelectionManager()->deselectAll();
    }
    
    // pass on mouse events. unless row is not selected
    getParentComponent()->mouseDown(e);
}
