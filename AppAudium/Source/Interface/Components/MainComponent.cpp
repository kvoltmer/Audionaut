/*
  ==============================================================================

  This is an automatically generated GUI class created by the Projucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Projucer version: 7.0.5

  ------------------------------------------------------------------------------

  The Projucer is part of the JUCE library.
  Copyright (c) 2020 - Raw Material Software Limited.

  ==============================================================================
*/

//[Headers] You can add your own extra header files here...
#include "WaveFormPanelComponent.h"
#include "Engine/AudiumEngine.h"
#include "RegionPanelComponent.h"
//[/Headers]

#include "MainComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
MainComponent::MainComponent (std::shared_ptr<AudiumEngine> audiumEngine)
{
    //[Constructor_pre] You can add your own custom stuff here..

    this->audiumEngine = audiumEngine;
    waveFormPanelComponent.reset(new WaveFormPanelComponent(audiumEngine));
    regionPanelComponent.reset(new RegionPanelComponent(audiumEngine));
    stretchableLayoutManager.reset(new StretchableLayoutManager());
    stretchableLayoutResizerBar.reset(new StretchableLayoutResizerBar(stretchableLayoutManager.get(), 1, true));

    //[/Constructor_pre]


    //[UserPreSize]
    //[/UserPreSize]

    setSize (1200, 400);


    //[Constructor] You can add your own custom stuff here..

    addAndMakeVisible(waveFormPanelComponent.get());
    addAndMakeVisible(stretchableLayoutResizerBar.get());
    addAndMakeVisible(regionPanelComponent.get());

    stretchableLayoutManager->setItemLayout (0,          // for item 0
                                             -0.0, -1.0,    // size must be between 0% and 100% of the available space
                                             -0.8);      // and its preferred size in % of the total available space

    stretchableLayoutManager->setItemLayout (1, // for item 1
                                             3, 3, 3);

    stretchableLayoutManager->setItemLayout (2,          // for item 2
                                             -0.1, -0.5, // size must be between 0% and 50% of the available space
                                             200);        // its preferred size in pixels

    resized();

    //[/Constructor]
}

MainComponent::~MainComponent()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
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

    // the list of components that we want to reposition
    Component* comps[] = {  waveFormPanelComponent.get(),
                            stretchableLayoutResizerBar.get(),
                            regionPanelComponent.get() };

    // this will position the 3 components, one above the other, to fit
    // horizontically into the rectangle provided.
    stretchableLayoutManager->layOutComponents (comps, 3,
                               0, 0, getWidth(), getHeight(),
                               false, true);

    //[/UserResized]
}

void MainComponent::filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY)
{
    //[UserCode_filesDropped] -- Add your code here...

    for (auto i = 0; i < filenames.size(); i++)
    {
        auto url = URL (File (filenames[i]));
        audiumEngine->getAudioResourceContainer()->addAudioResource(url);
    }
    updateUI();

    //[/UserCode_filesDropped]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...

void MainComponent::changeListenerCallback (ChangeBroadcaster* source)
{
}

void MainComponent::showAudioResource (URL resource)
{
}

bool MainComponent::loadURLIntoTransport (const URL& audioURL)
{
    return true;
}


bool MainComponent::isInterestedInFileDrag (const StringArray& /*files*/)
{
    return true;
}

void MainComponent::updateUI()
{
    waveFormPanelComponent->updateUI();
    regionPanelComponent->updateUI();
}

void MainComponent::zoomIn()
{
    waveFormPanelComponent->zoomIn();
}

void MainComponent::zoomOut()
{
    waveFormPanelComponent->zoomOut();
}

//[/MiscUserCode]


//==============================================================================
#if 0
/*  -- Projucer information section --

    This is where the Projucer stores the metadata that describe this GUI layout, so
    make changes in here at your peril!

BEGIN_JUCER_METADATA

<JUCER_COMPONENT documentType="Component" className="MainComponent" componentName=""
                 parentClasses="public juce::Component, private juce::ChangeListener, public FileDragAndDropTarget"
                 constructorParams="std::shared_ptr&lt;AudiumEngine&gt; audiumEngine"
                 variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
                 overlayOpacity="0.330" fixedSize="0" initialWidth="1200" initialHeight="400">
  <METHODS>
    <METHOD name="filesDropped (const juce::StringArray&amp; filenames, int mouseX, int mouseY)"/>
  </METHODS>
  <BACKGROUND backgroundColour="ff282829"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif


//[EndFile] You can add extra defines here...
//[/EndFile]

