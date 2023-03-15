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
#include "WaveFormComponent.h"
#include "Util/EngineAccess.h"
//[/Headers]

#include "MainComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
MainComponent::MainComponent (std::shared_ptr<AudiumEngine> audiumEngine)
{
    //[Constructor_pre] You can add your own custom stuff here..

    //zoomFactor = 1.0;

    thread.startThread();
    //[/Constructor_pre]

    zoomSlider.reset (new juce::Slider ("new slider"));
    addAndMakeVisible (zoomSlider.get());
    zoomSlider->setRange (0, 1, 0);
    zoomSlider->setSliderStyle (juce::Slider::LinearHorizontal);
    zoomSlider->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 80, 20);
    zoomSlider->addListener (this);
    zoomSlider->setSkewFactor (2);

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


    waveFormTableListBoxModel.reset(new WaveFormTableListBoxModel(audiumEngine->getAudioResourceContainer()));
    waveFormTableListBox.reset(new TableListBox("waveform table", waveFormTableListBoxModel.get()));
    waveFormTableListBox->setHeaderHeight(0);
    waveFormTableListBox->setRowHeight(200);
    waveFormTableListBox->getHeader().addColumn("waveform", 1, getWidth());
    addAndMakeVisible(waveFormTableListBox.get());
    waveFormTableListBox->setBounds(waveform__background->getBounds());
    waveFormTableListBox->setColour(juce::TableListBox::backgroundColourId, juce::Colour (0x00000000));

    audioDeviceManager.addAudioCallback (&audioSourcePlayer);
    audioSourcePlayer.setSource (&transportSource);
    //[/Constructor]
}

MainComponent::~MainComponent()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]

    zoomSlider = nullptr;
    waveform__background = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    transportSource  .setSource (nullptr);
    audioSourcePlayer.setSource (nullptr);

    audioDeviceManager.removeAudioCallback (&audioSourcePlayer);

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

    zoomSlider->setBounds ((getWidth() / 2), getHeight() - 10 - 24, 192, 24);
    waveform__background->setBounds (0, 0, proportionOfWidth (1.0000f), getHeight() - 50);
    //[UserResized] Add your own custom resize handling here..
    if (waveFormTableListBox != nullptr)
    {
        waveFormTableListBox->setBounds(waveform__background->getBounds());
        //waveFormTableListBox->getHeader().setColumnWidth(1, (waveform__background->getWidth() * zoomFactor));
        waveFormTableListBox->getHeader().setColumnWidth(1, (waveform__background->getWidth()));
    }

    //[/UserResized]
}

void MainComponent::sliderValueChanged (juce::Slider* sliderThatWasMoved)
{
    //[UsersliderValueChanged_Pre]
    //[/UsersliderValueChanged_Pre]

    if (sliderThatWasMoved == zoomSlider.get())
    {
        //[UserSliderCode_zoomSlider] -- add your slider handling code here..

        //zoomFactor = zoomSlider->getValue();
        //auto test = (1.0 - jlimit (0.0, 0.99, zoomFactor));
        //std::cout << test << std::endl;
        //auto newScale = jmax (0.001, audioResource->getThumbnail().getTotalLength() * (1.0 - jlimit (0.0, 0.99, amount)));
        //waveFormTableListBox->getHeader().setColumnWidth(1, (waveform__background->getWidth()));

        // alternative....
        waveFormTableListBoxModel->setZoomFactor(zoomSlider->getValue());
        waveFormTableListBox->updateContent();

        //[/UserSliderCode_zoomSlider]
    }

    //[UsersliderValueChanged_Post]
    //[/UsersliderValueChanged_Post]
}

void MainComponent::filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY)
{
    //[UserCode_filesDropped] -- Add your code here...

    auto url = URL (File (filenames[0]));
    auto resource = getAudiumEngine(this)->getAudioResourceContainer()->addAudioResource(url);

    waveFormTableListBox->updateContent();


    //[/UserCode_filesDropped]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...

void MainComponent::changeListenerCallback (ChangeBroadcaster* source)
{
//    if (source == waveFormComponent.get())
//        showAudioResource (URL (waveFormComponent->getLastDroppedFile()));
}

void MainComponent::showAudioResource (URL resource)
{
    if (loadURLIntoTransport (resource))
        currentAudioFile = std::move (resource);

    //zoomSlider.setValue (0, dontSendNotification);
}

bool MainComponent::loadURLIntoTransport (const URL& audioURL)
{
    // unload the previous file source and delete it..
    /// TODO:
//    transportSource.stop();
//    transportSource.setSource (nullptr);
//    currentAudioFileSource.reset();
//
//    const auto source = makeInputSource (audioURL);
//
//    if (source == nullptr)
//        return false;
//
//    auto stream = rawToUniquePtr (source->createInputStream());
//
//    if (stream == nullptr)
//        return false;
//
//    auto reader = rawToUniquePtr (formatManager.createReaderFor (std::move (stream)));
//
//    if (reader == nullptr)
//        return false;
//
//    currentAudioFileSource = std::make_unique<AudioFormatReaderSource> (reader.release(), true);
//
//    // ..and plug it into our transport source
//    transportSource.setSource (currentAudioFileSource.get(),
//                               32768,                   // tells it to buffer this many samples ahead
//                               &thread,                 // this is the background thread to use for reading-ahead
//                               currentAudioFileSource->getAudioFormatReader()->sampleRate);     // allows for sample rate correction

    return true;
}


bool MainComponent::isInterestedInFileDrag (const StringArray& /*files*/)
{
    return true;
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
  <SLIDER name="new slider" id="d8bc4db2e68bdf68" memberName="zoomSlider"
          virtualName="" explicitFocusOrder="0" pos="0C 10Rr 192 24" min="0.0"
          max="1.0" int="0.0" style="LinearHorizontal" textBoxPos="TextBoxLeft"
          textBoxEditable="1" textBoxWidth="80" textBoxHeight="20" skewFactor="2.0"
          needsCallback="1"/>
  <LABEL name="waveform background" id="3e76c9516fa31cfd" memberName="waveform__background"
         virtualName="" explicitFocusOrder="0" pos="0 0 100% 50M" bkgCol="ff292929"
         edTextCol="ff000000" edBkgCol="0" labelText="" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
         fontsize="15.0" kerning="0.0" bold="0" italic="0" justification="36"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif


//[EndFile] You can add extra defines here...
//[/EndFile]

