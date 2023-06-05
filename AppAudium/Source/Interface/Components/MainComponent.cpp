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
#include "Util/EngineAccess.h"
#include "Interface/Handlers/ZoomHandler.h"
//[/Headers]

#include "MainComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
MainComponent::MainComponent (std::shared_ptr<AudiumEngine> audiumEngine)
{
    //[Constructor_pre] You can add your own custom stuff here..

    this->audiumEngine = audiumEngine;

    //[/Constructor_pre]

    waveform__background.reset (new juce::Label ("waveform background",
                                                 juce::String()));
    addAndMakeVisible (waveform__background.get());
    waveform__background->setFont (juce::Font (15.00f, juce::Font::plain).withTypefaceStyle ("Regular"));
    waveform__background->setJustificationType (juce::Justification::centred);
    waveform__background->setEditable (false, false, false);
    waveform__background->setColour (juce::Label::backgroundColourId, juce::Colour (0xff292929));
    waveform__background->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    waveform__background->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));


    //[UserPreSize]
    //[/UserPreSize]

    setSize (1200, 400);


    //[Constructor] You can add your own custom stuff here..

    zoomHandler.reset(new ZoomHandler(audiumEngine->getAudioResourceContainer()));
    waveFormTableListBox.reset(new WaveFormTableListBox("waveform listbox", nullptr));
    regionSelector.reset(new RegionSelector(waveFormTableListBox, zoomHandler, audiumEngine->getAudioRegionContainer()));
    waveFormTableListBoxModel.reset(new WaveFormTableListBoxModel(waveFormTableListBox,
                                                                  audiumEngine->getAudioResourceContainer(),
                                                                  zoomHandler,
                                                                  regionSelector));
    waveFormTableListBox->setModel(waveFormTableListBoxModel.get());



    zoomHandler->setHorizontalScrollBar(&waveFormTableListBox->getHorizontalScrollBar());


    //waveFormTableListBox->setRowHeight(200);
    waveFormTableListBox->setMultipleSelectionEnabled(true);
    /// TODO: bug?
    waveFormTableListBox->setMinimumContentWidth(waveform__background->getWidth());
    waveFormTableListBox->setBounds(waveform__background->getBounds());
    waveFormTableListBox->setColour(juce::TableListBox::backgroundColourId, juce::Colour (0x00000000));
    addAndMakeVisible(waveFormTableListBox.get());

    // selection component

    //waveFormTableListBox->getViewport()->addAndMakeVisible(regionSelector.get());
    addAndMakeVisible(regionSelector.get());

    zoomHandler->setWidth(waveform__background->getWidth());

    updateUI();

    //[/Constructor]
}

MainComponent::~MainComponent()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]

    waveform__background = nullptr;


    //[Destructor]. You can add your own custom destruction code here..

    regionSelector = nullptr;
    waveFormTableListBox->setModel(nullptr);
    waveFormTableListBox = nullptr;
    waveFormTableListBoxModel = nullptr;
    zoomHandler = nullptr;



    //[/Destructor]
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (juce::Colour (0xff7b7c7d));

    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void MainComponent::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    waveform__background->setBounds (0, 0, proportionOfWidth (1.0000f), proportionOfHeight (1.0000f));
    //[UserResized] Add your own custom resize handling here..
    if (waveFormTableListBox != nullptr)
    {
        waveFormTableListBox->setBounds(waveform__background->getBounds());
    }

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
    waveFormTableListBox->updateContent();
}

void MainComponent::zoomIn()
{
    auto width = waveform__background->getWidth() * zoomHandler->zoomIn();
    waveFormTableListBox->setMinimumContentWidth(width);
    zoomHandler->setWidth(width);
    regionSelector->updateFromEngine();
}

void MainComponent::zoomOut()
{
    auto width = waveform__background->getWidth() * zoomHandler->zoomOut();
    waveFormTableListBox->setMinimumContentWidth(width);
    zoomHandler->setWidth(width);
    regionSelector->updateFromEngine();
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
  <BACKGROUND backgroundColour="ff7b7c7d"/>
  <LABEL name="waveform background" id="3e76c9516fa31cfd" memberName="waveform__background"
         virtualName="" explicitFocusOrder="0" pos="0 0 100% 100%" bkgCol="ff292929"
         edTextCol="ff000000" edBkgCol="0" labelText="" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
         fontsize="15.0" kerning="0.0" bold="0" italic="0" justification="36"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif


//[EndFile] You can add extra defines here...
//[/EndFile]

