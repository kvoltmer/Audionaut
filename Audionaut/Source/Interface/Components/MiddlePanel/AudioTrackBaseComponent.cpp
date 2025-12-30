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
        g.setColour (juce::Colours::white.withAlpha(0.5f));
        g.drawRoundedRectangle (getLocalBounds().toFloat(), 3.0f, 1.0f);
        
    }
    
}

void AudioTrackBaseComponent::filesDropped (const StringArray& filenames, int x, int y)
{
    if ( !filenames.isEmpty()) {
        auto position = zoomHandler->xToClocks(x);
        zoomHandler->snapToGrid(position);
        
        std::function<void (std::string)> callback = [](std::string error) {
            juce::NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                        "Failed to open File.",
                                                        "Failed to open: " + juce::String(error));
        };
        
        auto arrangementMode = audiumEngine->getPlayListScheduler()->isArrangementMode();
        audioTrack->addAudioFiles(filenames, position, arrangementMode, callback);
 
    }
    
    
    externalDragAndDrop = false;
    regionSelector->setEnabled(true);
    zoomHandler->getSnapToGridHandler()->clearRange();
    repaint();
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
    bool isSelected = audioTrack->isSelected();
    
    if (!e.mods.isAnyModifierKeyDown() && !isSelected) {
        audioTrack->getSelectionManager()->deselectAll();
    }
    
    // pass on mouse events. unless row is not selected
    getParentComponent()->mouseDown(e);
}
