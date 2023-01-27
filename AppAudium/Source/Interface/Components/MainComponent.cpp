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
//[/Headers]

#include "MainComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
MainComponent::MainComponent ()
{
    //[Constructor_pre] You can add your own custom stuff here..
    formatManager.registerBasicFormats();
    thread.startThread();
    
    //[/Constructor_pre]

    waveFormViewport.reset (new juce::Viewport ("waveform viewport"));
    addAndMakeVisible (waveFormViewport.get());

    zoomSlider.reset (new juce::Slider ("new slider"));
    addAndMakeVisible (zoomSlider.get());
    zoomSlider->setRange (0, 1, 0);
    zoomSlider->setSliderStyle (juce::Slider::LinearHorizontal);
    zoomSlider->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 80, 20);
    zoomSlider->addListener (this);
    zoomSlider->setSkewFactor (2);

    zoomSlider->setBounds (392, 280, 192, 24);


    //[UserPreSize]
    //[/UserPreSize]

    setSize (600, 400);


    //[Constructor] You can add your own custom stuff here..
    waveFormComponent.reset(new WaveFormComponent(formatManager, transportSource));
    waveFormComponent->setSize(getWidth(), waveFormViewport->getHeight());
    waveFormComponent->addChangeListener (this);

    waveFormViewport->setViewedComponent(waveFormComponent.get(), false);

    audioDeviceManager.addAudioCallback (&audioSourcePlayer);
    audioSourcePlayer.setSource (&transportSource);
    //[/Constructor]
}

MainComponent::~MainComponent()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]

    waveFormViewport = nullptr;
    zoomSlider = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    transportSource  .setSource (nullptr);
    audioSourcePlayer.setSource (nullptr);

    audioDeviceManager.removeAudioCallback (&audioSourcePlayer);

    waveFormComponent->removeChangeListener (this);
    //[/Destructor]
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (juce::Colour (0xff7bc7ed));

    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void MainComponent::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    waveFormViewport->setBounds (0, 0, proportionOfWidth (1.0000f), proportionOfHeight (0.4991f));
    //[UserResized] Add your own custom resize handling here..
    //[/UserResized]
}

void MainComponent::sliderValueChanged (juce::Slider* sliderThatWasMoved)
{
    //[UsersliderValueChanged_Pre]
    //[/UsersliderValueChanged_Pre]

    if (sliderThatWasMoved == zoomSlider.get())
    {
        //[UserSliderCode_zoomSlider] -- add your slider handling code here..
        waveFormComponent->setZoomFactor(zoomSlider->getValue());
        //[/UserSliderCode_zoomSlider]
    }

    //[UsersliderValueChanged_Post]
    //[/UsersliderValueChanged_Post]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...

void MainComponent::changeListenerCallback (ChangeBroadcaster* source)
{
    if (source == waveFormComponent.get())
        showAudioResource (URL (waveFormComponent->getLastDroppedFile()));
}

void MainComponent::showAudioResource (URL resource)
{
    if (loadURLIntoTransport (resource))
        currentAudioFile = std::move (resource);

    //zoomSlider.setValue (0, dontSendNotification);
    waveFormComponent->setURL (currentAudioFile);
}

bool MainComponent::loadURLIntoTransport (const URL& audioURL)
{
    // unload the previous file source and delete it..
    transportSource.stop();
    transportSource.setSource (nullptr);
    currentAudioFileSource.reset();

    const auto source = makeInputSource (audioURL);

    if (source == nullptr)
        return false;

    auto stream = rawToUniquePtr (source->createInputStream());

    if (stream == nullptr)
        return false;

    auto reader = rawToUniquePtr (formatManager.createReaderFor (std::move (stream)));

    if (reader == nullptr)
        return false;

    currentAudioFileSource = std::make_unique<AudioFormatReaderSource> (reader.release(), true);

    // ..and plug it into our transport source
    transportSource.setSource (currentAudioFileSource.get(),
                               32768,                   // tells it to buffer this many samples ahead
                               &thread,                 // this is the background thread to use for reading-ahead
                               currentAudioFileSource->getAudioFormatReader()->sampleRate);     // allows for sample rate correction

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
                 parentClasses="public juce::Component, private juce::ChangeListener"
                 constructorParams="" variableInitialisers="" snapPixels="8" snapActive="1"
                 snapShown="1" overlayOpacity="0.330" fixedSize="0" initialWidth="600"
                 initialHeight="400">
  <BACKGROUND backgroundColour="ff7bc7ed"/>
  <VIEWPORT name="waveform viewport" id="b74af6eff132eb11" memberName="waveFormViewport"
            virtualName="" explicitFocusOrder="0" pos="0 0 100% 49.911%"
            vscroll="1" hscroll="1" scrollbarThickness="8" contentType="0"
            jucerFile="" contentClass="" constructorParams=""/>
  <SLIDER name="new slider" id="d8bc4db2e68bdf68" memberName="zoomSlider"
          virtualName="" explicitFocusOrder="0" pos="392 280 192 24" min="0.0"
          max="1.0" int="0.0" style="LinearHorizontal" textBoxPos="TextBoxLeft"
          textBoxEditable="1" textBoxWidth="80" textBoxHeight="20" skewFactor="2.0"
          needsCallback="1"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif


//[EndFile] You can add extra defines here...
//[/EndFile]

