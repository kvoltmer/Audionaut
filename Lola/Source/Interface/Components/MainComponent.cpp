/*
  ==============================================================================

  This is an automatically generated GUI class created by the Projucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Projucer version: 7.0.8

  ------------------------------------------------------------------------------

  The Projucer is part of the JUCE library.
  Copyright (c) 2020 - Raw Material Software Limited.

  ==============================================================================
*/

//[Headers] You can add your own extra header files here...
#include "Interface/Components/HeaderPanel/HeaderComponent.h"
#include "Interface/Components/MiddlePanel/MiddlePanelComponent.h"
#include "Interface/Components/RightPanel/RightPanelComponent.h"

#include "Application/AudiumApplication.h"

#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/ActionMessages.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
//[/Headers]

#include "MainComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
MainComponent::MainComponent (std::shared_ptr<AudiumEngine> audiumEngine)
{
    //[Constructor_pre] You can add your own custom stuff here..

    this->audiumEngine = audiumEngine;

    headerComponent.reset(new HeaderComponent(audiumEngine->getPlayListScheduler()));
    middlePanelComponent.reset(new MiddlePanelComponent(audiumEngine));
    rightPanelComponent.reset(new RightPanelComponent(audiumEngine));
    stretchableLayoutManager.reset(new juce::StretchableLayoutManager());
    stretchableLayoutResizerBar.reset(new juce::StretchableLayoutResizerBar(stretchableLayoutManager.get(), 1, true));

    //[/Constructor_pre]


    //[UserPreSize]
    //[/UserPreSize]

    setSize (1200, 800);


    //[Constructor] You can add your own custom stuff here..

    addAndMakeVisible(headerComponent.get());
    addAndMakeVisible(middlePanelComponent.get());
    addAndMakeVisible(stretchableLayoutResizerBar.get());
    addAndMakeVisible(rightPanelComponent.get());

    stretchableLayoutManager->setItemLayout (0,          // for item 0
                                             25, -1.0,    // size must be between 25pix and 100% of the available space
                                             -0.8);      // and its preferred size in % of the total available space

    stretchableLayoutManager->setItemLayout (1, // for item 1
                                             2, 2, 2);

    stretchableLayoutManager->setItemLayout (2,          // for item 2
                                             25, -1.0, // size must be between 25pix and 50% of the available space
                                             400);        // its preferred size in pixels

    resized();

    audiumEngine->getAudioTrackContainer()->addActionListener(this);
    audiumEngine->getAudioResourceContainer()->addActionListener(this);
    audiumEngine->getPlayListScheduler()->getTempoProvider()->addActionListener(this);

    audiumEngine->getUndoManager()->addChangeListener(this);

    //[/Constructor]
}

MainComponent::~MainComponent()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    audiumEngine->getAudioTrackContainer()->removeActionListener(this);
    audiumEngine->getAudioResourceContainer()->removeActionListener(this);
    audiumEngine->getPlayListScheduler()->getTempoProvider()->removeActionListener(this);
    //[/Destructor_pre]



    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (juce::Colour (0xff282829));

    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void MainComponent::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    //[UserResized] Add your own custom resize handling here..

    const auto headerHeight = headerComponent->getHeight();
    headerComponent->setBounds(0, 0, getWidth(), headerHeight);

    // the list of components that we want to reposition
    Component* comps[] = {  middlePanelComponent.get(),
                            stretchableLayoutResizerBar.get(),
                            rightPanelComponent.get() };

    // this will position the 3 components, one above the other, to fit
    // horizontically into the rectangle provided.
    stretchableLayoutManager->layOutComponents (comps, 3,
                               0, headerHeight, getWidth(), getHeight() - headerHeight,
                               false, true);

    //[/UserResized]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...

void MainComponent::actionListenerCallback (const juce::String& message)
{
    //std::cout << "actionListenerCallback " << message.toStdString() << std::endl;

    if (message == regionCreatedAction)
    {
        middlePanelComponent->updateUI();
        rightPanelComponent->updateUI(RightPanelComponent::RegionListContext);
    }
    else if (message == regionClearedAction)
    {
        rightPanelComponent->clearSelection();
    }
    else if (message == regionModifiedAction)
    {
        rightPanelComponent->updateUI(RightPanelComponent::RegionListContext);
    }
    else if (message == regionSelectedAction)
    {
        middlePanelComponent->updateUI();
        rightPanelComponent->updateUI(RightPanelComponent::RegionListContext);
    }
    else if (message == playListItemCreatedAction)
    {
        middlePanelComponent->updateUI();
        rightPanelComponent->updateUI(RightPanelComponent::PlayListContext);
    }
    else if (message == playListItemTriggered)
    {
        rightPanelComponent->updateUI(RightPanelComponent::PlayListContext);
    }
    else if (message == playListItemSelection)
    {
        middlePanelComponent->updateUI();
        rightPanelComponent->updateUI();
    }
    else if (message == audioResourceCreatedAction)
    {
        middlePanelComponent->updateUI();
        rightPanelComponent->updateUI();
    }
    else if (message == audioTrackCreatedAction)
    {
        // TODO: update with context to rebuild everything
        updateUI();
    }
    else if (message == scrolledVertically)
    {
        middlePanelComponent->updateUI(MiddlePanelComponent::VerticalScrollContext);
    }
    else if (message == rebuildAll)
    {
        middlePanelComponent->updateUI(MiddlePanelComponent::ForceRebuildContext);
        rightPanelComponent->updateUI();
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
    middlePanelComponent->updateUI(MiddlePanelComponent::ForceRebuildContext);
    rightPanelComponent->updateUI();
}

void MainComponent::updateUI()
{
    auto editMode = audiumEngine->getPlayListScheduler()->isEditMode();
    middlePanelComponent->showArrangementComponent(!editMode);
    middlePanelComponent->showEditComponent(editMode);

    middlePanelComponent->updateUI();
    rightPanelComponent->updateUI();

    updateWindowTitle();
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
    audiumEngine->getAudioTrackContainer()->getSelectionManager()->pasteFromClipboard(audiumEngine);
    updateUI();
}

void MainComponent::duplicate()
{
    updateUI();
}

//[/MiscUserCode]


//==============================================================================
#if 0
/*  -- Projucer information section --

    This is where the Projucer stores the metadata that describe this GUI layout, so
    make changes in here at your peril!

BEGIN_JUCER_METADATA

<JUCER_COMPONENT documentType="Component" className="MainComponent" componentName=""
                 parentClasses="public juce::Component, private juce::ActionListener, private juce::ChangeListener"
                 constructorParams="std::shared_ptr&lt;AudiumEngine&gt; audiumEngine"
                 variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
                 overlayOpacity="0.330" fixedSize="0" initialWidth="1200" initialHeight="800">
  <BACKGROUND backgroundColour="ff282829"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif


//[EndFile] You can add extra defines here...
//[/EndFile]

