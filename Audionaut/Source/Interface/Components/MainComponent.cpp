//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Engine/AudiumEngine.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/ActionMessages.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Export/PlayListItemExport.h"
#include "Engine/Recording/RecordingActionHandler.h"

#include "Interface/Components/HeaderPanel/HeaderComponent.h"
#include "Interface/Components/MiddlePanel/MiddlePanelComponent.h"
#include "Interface/Components/RightPanel/RightPanelComponent.h"
#include "Interface/Controls/PanelResizerBar.h"
#include "Util/Preferences.h"
#include "Interface/Models/PlayListTableListBoxItem.h"

#include "Application/AudiumApplication.h"



#include "MainComponent.h"

using namespace audium;

//==============================================================================
MainComponent::MainComponent (std::shared_ptr<audium::AudiumEngine> audiumEngine_) :
    audiumEngine(audiumEngine_)
{
    headerComponent = std::make_unique<HeaderComponent>(audiumEngine);
    headerComponent->onRightPanelButtonClick = [this] { setRightPanelVisible (! isRightPanelVisible()); };
    headerComponent->onMixerButtonClick = [this] { setMixerVisible (! isMixerVisible()); };

    middlePanelComponent.reset(new MiddlePanelComponent(audiumEngine));
    middlePanelComponent->onMixerLayoutChanged = [this]
    {
        writeLayoutPreferences();
        updatePanelToggles();   // a mixer toggle lays out the middle panel only, so the header hears it from here
    };
    rightPanelComponent.reset(new RightPanelComponent(audiumEngine));

    // the right panel leaves the middle panel its minimum width; its open state
    // and width are the user's and are persisted
    rightPanel.maximumSize = [this] { return getWidth() - minMiddlePanelWidth - PanelResizerBar::seamThickness; };
    rightPanel.onRelayout = [this] { resized(); };
    rightPanel.onUserChange = [this]
    {
        writeLayoutPreferences();
        updatePanelToggles();
    };

    // the seam between the middle and the right panel: dragging it left widens the
    // right panel, dragging it well past the minimum collapses the panel until dragged back or released
    rightPanelResizerBar = std::make_unique<PanelResizerBar> (true);
    rightPanelResizerBar->onDragStart = [this] { rightPanel.dragStarted(); };
    rightPanelResizerBar->onDrag = [this] (int distance) { rightPanel.dragged (distance); };
    rightPanelResizerBar->onDragEnd = [this] { rightPanel.dragEnded(); };
    rightPanelResizerBar->onDoubleClick = [this] { rightPanel.resetToDefault(); };

    readLayoutPreferences();

    setSize (1280, 800);

    addAndMakeVisible(headerComponent.get());
    addAndMakeVisible(middlePanelComponent.get());
    addAndMakeVisible(rightPanelComponent.get());
    addAndMakeVisible(rightPanelResizerBar.get());   // after its neighbours: it overlaps their edges

    resized();

    audiumEngine->getAudioTrackContainer()->addActionListener(this);
    audiumEngine->getAudioResourceContainer()->addActionListener(this);
    audiumEngine->getPlayListScheduler()->getTempoProvider()->addActionListener(this);

    audiumEngine->getUndoManager()->addChangeListener(this);
}

MainComponent::~MainComponent()
{
    audiumEngine->getAudioTrackContainer()->removeActionListener(this);
    audiumEngine->getAudioResourceContainer()->removeActionListener(this);
    audiumEngine->getPlayListScheduler()->getTempoProvider()->removeActionListener(this);

    // the engine - and with it the undo manager - outlives this component at
    // shutdown: unregister, or the next undo broadcast reaches a dead listener
    audiumEngine->getUndoManager()->removeChangeListener(this);
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff282829));
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    headerComponent->setBounds (bounds.removeFromTop (HeaderComponent::preferredHeight));

    const auto rightPanelWidth = rightPanel.getShownSize();
    rightPanelComponent->setVisible (rightPanelWidth > 0);
    rightPanelResizerBar->setVisible (rightPanelWidth > 0 || rightPanel.isDragging());   // a drag keeps its seam through a collapse
    if (rightPanelWidth > 0)
        rightPanelComponent->setBounds (bounds.removeFromRight (rightPanelWidth));
    if (rightPanelResizerBar->isVisible())
        rightPanelResizerBar->setBounds (PanelResizerBar::boundsForSeam (bounds.removeFromRight (PanelResizerBar::seamThickness), true));

    middlePanelComponent->setBounds (bounds);
}

void MainComponent::setMixerVisible (bool visible)
{
    middlePanelComponent->setMixerVisible (visible);   // persists through onMixerLayoutChanged
}

bool MainComponent::isMixerVisible() const
{
    return middlePanelComponent->isMixerVisible();
}

void MainComponent::updatePanelToggles()
{
    headerComponent->setPanelStates (rightPanel.isOpen(), isMixerVisible());
}

void MainComponent::readLayoutPreferences()
{
    auto& preferences = AudiumApplication::getPreferences();
    const auto intValue = [&] (const char* key, int defaultValue)
    {
        return String (preferences.getValue (key, std::to_string (defaultValue))).getIntValue();
    };

    rightPanel.restore (preferences.getValue (PreferenceKeys::rightPanelVisible, "true") == "true",
                        intValue (PreferenceKeys::rightPanelWidth, defaultRightPanelWidth));
    middlePanelComponent->restoreMixerLayout (preferences.getValue (PreferenceKeys::mixerVisible, "false") == "true",
                                              intValue (PreferenceKeys::mixerHeight, MiddlePanelComponent::defaultMixerHeight));
    updatePanelToggles();
}

void MainComponent::writeLayoutPreferences()
{
    auto& preferences = AudiumApplication::getPreferences();
    preferences.setValue (PreferenceKeys::rightPanelVisible, rightPanel.isOpen() ? "true" : "false");
    preferences.setValue (PreferenceKeys::rightPanelWidth, std::to_string (rightPanel.getSize()));
    preferences.setValue (PreferenceKeys::mixerVisible, middlePanelComponent->isMixerVisible() ? "true" : "false");
    preferences.setValue (PreferenceKeys::mixerHeight, std::to_string (middlePanelComponent->getMixerHeight()));
}

void MainComponent::actionListenerCallback (const juce::String& message)
{

    if (message == audium::scrolledVertically)
    {
        middlePanelComponent->updateUI(MiddlePanelComponent::VerticalScrollContext);
    }
    else if (message == audium::rebuildAll)
    {
        rebuildUI();
        updateUI();
    }
    else if (message == audium::updateAll)
    {
        updateUI();
    }
    else if (message == audium::updateMiddlePanelAction)
    {
        middlePanelComponent->updateUI();
    }
    else if (message == audium::updateChannelsAction)
    {
        // mixer parameter changed (undo/redo included): only the channel
        // strips need refreshing, the arrangement is untouched
        middlePanelComponent->updateUI(MiddlePanelComponent::ChannelsContext);
    }
    else if (message == audium::updateRightPanelAction)
    {
        rightPanelComponent->updateUI();
    }
    else if (message == audium::updateArrangementAction)
    {
        middlePanelComponent->updateUI(MiddlePanelComponent::ArrangementContext);
    }
    else if (message == audium::tempoChanged)
    {
        // tempo-locked clips change their timeline extent with the tempo
        middlePanelComponent->updateUI(MiddlePanelComponent::ArrangementContext);
    }
    else if (message == audium::updateSelection)
    {
        middlePanelComponent->updateUI(MiddlePanelComponent::ArrangementContext);
        rightPanelComponent->updateUI(SelectionContext);
    }
    else if (message == audium::recordingFinishedAction) {
        audiumEngine->getRecordingActionHandler()->onRecordingFinished();
        audiumEngine->getPlayListScheduler()->commitPlayListData();
        updateUI();
    }
    else if (message == audium::transportLoopAction) {
        if (audiumEngine->getPlayListScheduler()->isRecording()) {
            audiumEngine->getRecordingActionHandler()->onLoopAction();
            middlePanelComponent->updateUI(MiddlePanelComponent::ArrangementContext);
        }
    }
    else if (message == audium::transportLoopEntered) {
        if (audiumEngine->getPlayListScheduler()->isRecording()) {
            audiumEngine->getRecordingActionHandler()->onLoopEntered();
            middlePanelComponent->updateUI(MiddlePanelComponent::ArrangementContext);
        }
    }
    
    else // update everything (eg. region deleted)
    {
        updateUI();
    }
}

void MainComponent::changeListenerCallback (ChangeBroadcaster* source)
{
    updateWindowTitle();
}

void MainComponent::rebuildUI()
{
    headerComponent->updateUI();
    middlePanelComponent->updateUI(MiddlePanelComponent::ForceRebuildContext);
    rightPanelComponent->updateUI(RebuildContext);
}

void MainComponent::writeUiState(json& uiState)
{
    middlePanelComponent->writeUiState(uiState);
}

void MainComponent::readUiState(const json& uiState)
{
    middlePanelComponent->readUiState(uiState);
}

void MainComponent::updateUI()
{
    headerComponent->updateUI();
    middlePanelComponent->updateUI();
    rightPanelComponent->updateUI(ContentContext);

    updateWindowTitle();
    
    if (auto audiumLookAndFeel = dynamic_cast<AudiumLookAndFeel*>(&getLookAndFeel())) {
        for (auto audioTrack : audiumEngine->getAudioTrackContainer()->getAudioTracks()) {
            if (audioTrack->getId() < AudiumLookAndFeel::maxTrackColours)
                audiumLookAndFeel->trackColours[audioTrack->getId()] = audioTrack->getViewState().getColour();
        }
    }
}

void MainComponent::updateWindowTitle()
{
#if !defined(AUDIONAUT_HEADLESS)
    if (getParentComponent() != nullptr)
    {
        auto projectName = audiumEngine->getProjectFileStore()->getCurrentProjectFile().getFileName();
        if (projectName.isEmpty()) {
            projectName = "Untitled";
        }
        else {
            if (ProjectFileStore::isValidProjectStructure(audiumEngine->getProjectFileStore()->getCurrentProjectFile().getParentDirectory())) {
                projectName = audiumEngine->getProjectFileStore()->getCurrentProjectFile().getParentDirectory().getFileName();
            }
        }
        if (audiumEngine->getProjectFileStore()->wasChangedExternally())
            projectName += " - " + TRANS("Edited By Agent");
        else if (audiumEngine->getUndoManager()->canUndo())
            projectName += " - Edited";

        getParentComponent()->setName(projectName);
    }
#endif
}

void MainComponent::zoomIn()
{
    middlePanelComponent->zoomIn();
}

void MainComponent::zoomOut()
{
    middlePanelComponent->zoomOut();
}

void MainComponent::pageLeft()
{
    middlePanelComponent->pageLeft();
}

void MainComponent::pageRight()
{
    middlePanelComponent->pageRight();
}

void MainComponent::selectAll()
{
    for (auto track : audiumEngine->getAudioTrackContainer()->getAudioTracks()){
        track->setSelected(true);
    }
    updateUI();
}

void MainComponent::copy()
{
    audiumEngine->getAudioTrackContainer()->getSelectionManager()->copySelectedToClipboard();
}

void MainComponent::paste()
{
    audiumEngine->getAudioTrackContainer()->getSelectionManager()->pasteFromClipboard(audiumEngine, false);
}

void MainComponent::duplicate()
{
    audiumEngine->getAudioTrackContainer()->getSelectionManager()->copySelectedToClipboard();
    audiumEngine->getAudioTrackContainer()->getSelectionManager()->pasteFromClipboard(audiumEngine, true);
}

bool MainComponent::shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails& sourceDetails,
                                           juce::StringArray& files,
                                           bool& canMoveFiles)
{
    // only PlayListTableListBoxItem is supported
    if (dynamic_cast<PlayListTableListBoxItem*>(sourceDetails.sourceComponent.get())) {
        
        
        /// make sure the temp project directory exists
        audiumEngine->getAudioResourceContainer()->createTemporaryProjectDirectory(false);
        
        auto selectionManager = audiumEngine->getAudioTrackContainer()->getSelectionManager();
        
        auto selectedObjects = selectionManager->getSelectedObjects();
        for (auto object : selectedObjects) {
            if (auto playListItem = std::dynamic_pointer_cast<PlayListItem>(object)) {
                jassert(playListItem->isSelected());
                auto exporter = std::make_unique<audium::PlayListItemExport>(audiumEngine,
                                                                             playListItem,
                                                                             false);
                if (exporter->exportItem()) {
                    files.add(exporter->config->fileName.getFullPathName());
                }
            }
        }
        canMoveFiles = true;
        
    }
    return files.size() > 0;
}
