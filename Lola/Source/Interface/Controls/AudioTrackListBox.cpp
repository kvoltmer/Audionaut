
#include <JuceHeader.h>
#include "AudioTrackListBox.h"

#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/PlayList/PlayListScheduler.h"

#include "Interface/Handlers/SnapToGridHandler.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Components/MiddlePanel/ArrangementView/PlayListItemComponent.h"
#include "Interface/Controls/DraggerControl.h"
#include "Interface/Controls/RegionLabel.h"
#include "Interface/Controls/TableRegionLabel.h"

using namespace audium;


AudioTrackListBox::AudioTrackListBox (std::shared_ptr<AudiumEngine> audiumEngine,
                                      std::shared_ptr<ZoomHandler> zoomHandler) :
    audium::ListBox("AudioTrackListBox", nullptr),
    audiumEngine(audiumEngine),
    zoomHandler(zoomHandler)
{
    // background is transparent
    setColour(ListBox::backgroundColourId, juce::Colours::transparentBlack);
}

AudioTrackListBox::~AudioTrackListBox()
{
}

void AudioTrackListBox::setNewGroupColour(std::shared_ptr<AudioTrack> audioTrack)
{
    if (audioTrack->getColour() == juce::Colours::pink)
    {
        audioTrack->setColour(audiumEngine->getAudioTrackContainer()->getNewAudioTrackColour());
    }
}

void AudioTrackListBox::filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY)
{
    if ( !filenames.isEmpty()) {
        auto action = std::make_unique<audium::UndoableContainerAction>(*audiumEngine->getAudioTrackContainer());
        
        auto audioTrack = audiumEngine->getAudioTrackContainer()->createNewAudioTrack(juce::String());
        setNewGroupColour(audioTrack);
                
        auto position = zoomHandler->xToClocks(mouseX);
        zoomHandler->snapToGrid(position);
        
        std::function<void (std::string)> callback = [this, audioTrack](std::string error) {
            audiumEngine->getAudioTrackContainer()->deleteAudioTrack(audioTrack);
            
            juce::NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                        "Failed to open File.",
                                                        "Failed to open: " + juce::String(error));
        };
        
        bool arrangementMode = audiumEngine->getPlayListScheduler()->isArrangementMode();
        if (audioTrack->addAudioFiles(filenames, position, arrangementMode, callback)) {
            action->storeNewState();
            audiumEngine->getUndoManager()->perform(action.release(), "File(s) dropped");
            audiumEngine->getUndoManager()->beginNewTransaction();
        }
    }
    externalDragAndDrop = false;
    repaint();
}

void AudioTrackListBox::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    externalDragAndDrop = true;
    repaint();
}

void AudioTrackListBox::fileDragMove (const StringArray& files, int x, int y)
{
    auto start = zoomHandler->xToClocks(x);
    auto end = start + 0.01;
    Range<double> rangeInClocks(start, end);
    
    zoomHandler->getSnapToGridHandler()->publishRange(rangeInClocks);
}

void AudioTrackListBox::fileDragExit (const juce::StringArray& files)
{
    externalDragAndDrop = false;
    zoomHandler->getSnapToGridHandler()->clearRange();
    repaint();
}

bool AudioTrackListBox::isInterestedInDragSource (const SourceDetails &dragSourceDetails)
{
    if (dynamic_cast<PlayListItemComponent*>(dragSourceDetails.sourceComponent.get()) != nullptr)
        return true;

    if (dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr)
        return true;
    
    if (dynamic_cast<TableRegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr)
        return true;
    
    return false;
}

void AudioTrackListBox::itemDragEnter (const SourceDetails &dragSourceDetails)
{
    externalDragAndDrop = true;
    repaint();
}

void AudioTrackListBox::itemDragMove (const SourceDetails &dragSourceDetails)
{
    auto x = dragSourceDetails.localPosition.x;
    auto length = 0.01;
    if (auto playListItemComponent = dynamic_cast<PlayListItemComponent*>(dragSourceDetails.sourceComponent.get()))
    {
        // apply x offset
        x -= playListItemComponent->getDraggerControl()->mouseDownOffset.getX();
        if (auto region = playListItemComponent->getPlayListItem()->getRegion())
        {
            length = region->getRegionData(audium::clocks).getLength();
        }
    }

    auto start = zoomHandler->xToClocksWithOffset(x);
    auto end = start + length;
    Range<double> rangeInClocks(start, end);
    
    zoomHandler->getSnapToGridHandler()->publishRange(rangeInClocks);
}

void AudioTrackListBox::itemDragExit (const SourceDetails &dragSourceDetails)
{
    externalDragAndDrop = false;
    zoomHandler->getSnapToGridHandler()->clearRange();
    repaint();
}

void AudioTrackListBox::itemDropped (const SourceDetails &dragSourceDetails)
{
    // undo
    auto action = std::make_unique<audium::UndoableContainerAction>(*audiumEngine->getAudioTrackContainer());
    
    auto x = dragSourceDetails.localPosition.x;
    
    // apply x offset
    if (auto playListItemComponent = dynamic_cast<PlayListItemComponent*>(dragSourceDetails.sourceComponent.get()))
        x -= playListItemComponent->getDraggerControl()->mouseDownOffset.getX();
        
    auto pos = zoomHandler->xToClocksWithOffset(x);
    zoomHandler->snapToGrid(pos);
    
    bool success = false;
    
    if (dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr ||
        dynamic_cast<TableRegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr) {
        auto audioTrack = audiumEngine->getAudioTrackContainer()->createNewAudioTrack(juce::String());
        setNewGroupColour(audioTrack);
        audioTrack->dropSelectedAudioRegions(pos, audium::clocks);
        success = true;
    }
    else if (auto playListItemComponent = dynamic_cast<PlayListItemComponent*>(dragSourceDetails.sourceComponent.get())) {
        auto audioTrack = audiumEngine->getAudioTrackContainer()->createNewAudioTrack(juce::String());
        setNewGroupColour(audioTrack);
        audioTrack->dropPlayListItem(playListItemComponent->getPlayListItem(), pos, audium::clocks);
        success = true;
    }
    
    if (success) {
        action->storeNewState();
        auto undoManager = audiumEngine->getAudioTrackContainer()->getUndoManager();
        undoManager->perform(action.release(), "item dropped");
        undoManager->beginNewTransaction();
    }
    
    zoomHandler->getSnapToGridHandler()->clearRange();
    externalDragAndDrop = false;
    repaint();
}


void AudioTrackListBox::paint (juce::Graphics& g)
{
    if (externalDragAndDrop) {
        auto height = getHeaderComponent()->getHeight();
        for (auto r = 0; r < getModel()->getNumRows(); r++) {
            height += getModel()->getRowHeight(r);
        }
        g.setColour(findColour(audium::secondaryBackgroundColourId).brighter().withAlpha(0.5f));

        // TODO: missing vertical offset?
        g.fillRect(0, height, getWidth(), std::max(0, getHeight() - height));
    }

    
    audium::ListBox::paint(g);
}

