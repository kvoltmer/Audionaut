//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.


#include "Interface/Components/HeaderPanel/HeaderComponent.h"
#include "Interface/Components/MiddlePanel/MiddlePanelComponent.h"
#include "Interface/Components/RightPanel/RightPanelComponent.h"

#include "Application/AudiumApplication.h"

#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/ActionMessages.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"

#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent (std::shared_ptr<AudiumEngine> audiumEngine_) :
    audiumEngine(audiumEngine_)
{
    headerComponent = std::make_unique<HeaderComponent>(audiumEngine);
    headerComponent->onRightPanelButtonClick = [this]() {
        rightPanelVisible = !rightPanelVisible;
        resized();
    };
    
    middlePanelComponent.reset(new MiddlePanelComponent(audiumEngine));
    rightPanelComponent.reset(new RightPanelComponent(audiumEngine));
    stretchableLayoutManager.reset(new juce::StretchableLayoutManager());
    stretchableLayoutResizerBar.reset(new juce::StretchableLayoutResizerBar(stretchableLayoutManager.get(), 1, true));


    setSize (1200, 800);

    addAndMakeVisible(headerComponent.get());
    addAndMakeVisible(middlePanelComponent.get());
    addAndMakeVisible(stretchableLayoutResizerBar.get());
    addAndMakeVisible(rightPanelComponent.get());

    stretchableLayoutManager->setItemLayout (0,          // for item 0
                                             25, -1.0,    // size must be between 25pix and 100% of the available space
                                             -0.8);      // and its preferred size in % of the total available space

    stretchableLayoutManager->setItemLayout (1, // for item 1
                                             3, 3, 3);

    stretchableLayoutManager->setItemLayout (2,          // for item 2
                                             25, -1.0, // size must be between 25pix and 50% of the available space
                                             400);        // its preferred size in pixels

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
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff282829));
}

void MainComponent::resized()
{
    const auto headerHeight = headerComponent->getHeight();
    headerComponent->setBounds(0, 0, getWidth(), headerHeight);
    
    stretchableLayoutResizerBar->setVisible(rightPanelVisible);
    rightPanelComponent->setVisible(rightPanelVisible);
    
    if (rightPanelVisible) {
        
        // the list of components that we want to reposition
        Component* comps[] = {  middlePanelComponent.get(),
                                stretchableLayoutResizerBar.get(),
                                rightPanelComponent.get() };
        
        // this will position the 3 components, one above the other, to fit
        // horizontically into the rectangle provided.
        stretchableLayoutManager->layOutComponents (comps, 3,
                                                    0, headerHeight, getWidth(), getHeight() - headerHeight,
                                                    false, true);
    }
    else {
        middlePanelComponent->setBounds(0,
                                        headerHeight,
                                        getWidth(),
                                        getHeight() - headerHeight);
    }
}

void MainComponent::actionListenerCallback (const juce::String& message)
{
    //std::cout << "actionListenerCallback " << message.toStdString() << std::endl;

    if (message == scrolledVertically)
    {
        middlePanelComponent->updateUI(MiddlePanelComponent::VerticalScrollContext);
    }
    else if (message == rebuildAll)
    {
        rebuildUI();
        updateUI();
    }
    else if (message == updateAll)
    {
        updateUI();
    }
    else if (message == updateMiddlePanelAction)
    {
        middlePanelComponent->updateUI();
    }
    else if (message == updateRightPanelAction)
    {
        rightPanelComponent->updateUI();
    }
    else if (message == updateArrangementAction)
    {
        middlePanelComponent->updateUI(MiddlePanelComponent::ArrangementContext);
    }
    else if (message == updateSelection)
    {
        middlePanelComponent->updateUI(MiddlePanelComponent::ArrangementContext);
        rightPanelComponent->updateUI(SelectionContext);
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

void MainComponent::updateUI()
{
    headerComponent->updateUI();
    auto editMode = audiumEngine->getPlayListScheduler()->isEditMode();
    middlePanelComponent->showArrangementComponent(!editMode);
    middlePanelComponent->showEditComponent(editMode);

    middlePanelComponent->updateUI();
    rightPanelComponent->updateUI(ContentContext);

    updateWindowTitle();
    
    if (auto audiumLookAndFeel = dynamic_cast<AudiumLookAndFeel*>(&getLookAndFeel())) {
        for (auto audioTrack : audiumEngine->getAudioTrackContainer()->getAudioTracks()) {
            if (audioTrack->getId() < AudiumLookAndFeel::maxTrackColours)
                audiumLookAndFeel->trackColours[audioTrack->getId()] = audioTrack->getColour();
        }
    }
}

void MainComponent::updateWindowTitle()
{
#if !defined(CATCH2_TESTS)
    if (getParentComponent() != nullptr)
    {
        auto fileName = audiumEngine->getCurrentFile().getFileNameWithoutExtension();
        if (fileName.isEmpty())
            fileName = "Untitled";

        if (audiumEngine->getUndoManager()->canUndo())
            fileName += " *";

        auto appName = AudiumApplication::getApp().getApplicationName();
        getParentComponent()->setName(fileName + " - " + appName);
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

void MainComponent::toggleEditArrangementComponent()
{
    // toggle edit mode
    auto editMode = !audiumEngine->getPlayListScheduler()->isEditMode();
    audiumEngine->getPlayListScheduler()->setEditMode(editMode);
    audiumEngine->getUiState()["editMode"] = editMode;
    updateUI();
}

void MainComponent::selectAll()
{
    for (auto track : audiumEngine->getAudioTrackContainer()->getAudioTracks())
    {
        track->setSelected(true, true);
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
