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
#include "Engine/PlayList/PlayListScheduler.h"

//[/Headers]

#include "HeaderComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
HeaderComponent::HeaderComponent (std::shared_ptr<PlayListScheduler> playListScheduler)
{
    //[Constructor_pre] You can add your own custom stuff here..
    this->playListScheduler = playListScheduler;
    //[/Constructor_pre]

    link__textButton.reset (new juce::TextButton ("Link"));
    addAndMakeVisible (link__textButton.get());
    link__textButton->addListener (this);
    link__textButton->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff7a7a7a));
    link__textButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff12a4e2));

    link__textButton->setBounds (5, 10, 70, 20);

    tempo__label.reset (new juce::Label ("new label",
                                         TRANS ("120.0")));
    addAndMakeVisible (tempo__label.get());
    tempo__label->setFont (juce::Font (13.00f, juce::Font::plain).withTypefaceStyle ("Regular"));
    tempo__label->setJustificationType (juce::Justification::centred);
    tempo__label->setEditable (true, true, false);
    tempo__label->setColour (juce::Label::backgroundColourId, juce::Colours::grey);
    tempo__label->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    tempo__label->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));
    tempo__label->addListener (this);

    tempo__label->setBounds (496, 10, 70, 20);

    bars__label.reset (new juce::Label ("new label",
                                        TRANS ("0")));
    addAndMakeVisible (bars__label.get());
    bars__label->setFont (juce::Font (13.00f, juce::Font::plain).withTypefaceStyle ("Regular"));
    bars__label->setJustificationType (juce::Justification::centredRight);
    bars__label->setEditable (true, true, false);
    bars__label->setColour (juce::Label::backgroundColourId, juce::Colours::grey);
    bars__label->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    bars__label->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));
    bars__label->addListener (this);

    bars__label->setBounds (235, 10, 70, 20);

    beats__label.reset (new juce::Label ("new label",
                                         TRANS ("0")));
    addAndMakeVisible (beats__label.get());
    beats__label->setFont (juce::Font (13.00f, juce::Font::plain).withTypefaceStyle ("Regular"));
    beats__label->setJustificationType (juce::Justification::centredRight);
    beats__label->setEditable (true, true, false);
    beats__label->setColour (juce::Label::backgroundColourId, juce::Colours::grey);
    beats__label->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    beats__label->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));
    beats__label->addListener (this);

    beats__label->setBounds (308, 10, 35, 20);

    rest__label.reset (new juce::Label ("new label",
                                        TRANS ("0")));
    addAndMakeVisible (rest__label.get());
    rest__label->setFont (juce::Font (13.00f, juce::Font::plain).withTypefaceStyle ("Regular"));
    rest__label->setJustificationType (juce::Justification::centredRight);
    rest__label->setEditable (true, true, false);
    rest__label->setColour (juce::Label::backgroundColourId, juce::Colours::grey);
    rest__label->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    rest__label->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));
    rest__label->addListener (this);

    rest__label->setBounds (346, 10, 35, 20);


    //[UserPreSize]
    //[/UserPreSize]

    setSize (1200, 40);


    //[Constructor] You can add your own custom stuff here..

    link__textButton->onClick = [this] { this->playListScheduler->getLinkEngine()->mLink.enable(link__textButton->getToggleState()); };

    link__textButton->setClickingTogglesState(true);

    startTimerHz(60.f);

    //[/Constructor]
}

HeaderComponent::~HeaderComponent()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]

    link__textButton = nullptr;
    tempo__label = nullptr;
    bars__label = nullptr;
    beats__label = nullptr;
    rest__label = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void HeaderComponent::paint (juce::Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (juce::Colour (0xff2b2b2b));

    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void HeaderComponent::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    //[UserResized] Add your own custom resize handling here..
    //[/UserResized]
}

void HeaderComponent::buttonClicked (juce::Button* buttonThatWasClicked)
{
    //[UserbuttonClicked_Pre]
    //[/UserbuttonClicked_Pre]

    if (buttonThatWasClicked == link__textButton.get())
    {
        //[UserButtonCode_link__textButton] -- add your button handler code here..
        auto state = link__textButton->getToggleStateValue();
        //[/UserButtonCode_link__textButton]
    }

    //[UserbuttonClicked_Post]
    //[/UserbuttonClicked_Post]
}

void HeaderComponent::labelTextChanged (juce::Label* labelThatHasChanged)
{
    //[UserlabelTextChanged_Pre]
    //[/UserlabelTextChanged_Pre]

    if (labelThatHasChanged == tempo__label.get())
    {
        //[UserLabelCode_tempo__label] -- add your label text handling code here..
        playListScheduler->setTempo(tempo__label->getText().getDoubleValue());
        //[/UserLabelCode_tempo__label]
    }
    else if (labelThatHasChanged == bars__label.get())
    {
        //[UserLabelCode_bars__label] -- add your label text handling code here..
        //[/UserLabelCode_bars__label]
    }
    else if (labelThatHasChanged == beats__label.get())
    {
        //[UserLabelCode_beats__label] -- add your label text handling code here..
        //[/UserLabelCode_beats__label]
    }
    else if (labelThatHasChanged == rest__label.get())
    {
        //[UserLabelCode_rest__label] -- add your label text handling code here..
        //[/UserLabelCode_rest__label]
    }

    //[UserlabelTextChanged_Post]
    //[/UserlabelTextChanged_Post]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...

void HeaderComponent::timerCallback()
{
    const auto numPeers = playListScheduler->getLinkEngine()->mLink.numPeers();
    juce::String txt;
    if (numPeers > 0)
    {
        txt = juce::String(numPeers) + " Link";
        if (numPeers > 1)
        {
            txt += "s";
        }
    }
    else
    {
        txt = "Link";
    }
    link__textButton->setButtonText(txt);

    if (not tempo__label->isBeingEdited())
    {
        const auto tempo = playListScheduler->getTempo();
        tempo__label->setText(juce::String(tempo, 2), juce::dontSendNotification);
    }

    //auto beats = playListScheduler->getLinkEngine()->beatTime();
    const auto clocks = playListScheduler->getAbsolutePositionClocks();

    const auto beats = PlayListScheduler::clocksToBeats(clocks);
    const auto bars = static_cast<int>(beats / 4.0) + 1;
    bars__label->setText(juce::String(bars), juce::dontSendNotification);

    const auto beatsMod = (static_cast<int>(beats) % 4) + 1;
    beats__label->setText(juce::String(beatsMod), juce::dontSendNotification);

    const auto clocksMod = (static_cast<int>(clocks) % 96) + 1;
    rest__label->setText(juce::String(clocksMod), juce::dontSendNotification);

}

//[/MiscUserCode]


//==============================================================================
#if 0
/*  -- Projucer information section --

    This is where the Projucer stores the metadata that describe this GUI layout, so
    make changes in here at your peril!

BEGIN_JUCER_METADATA

<JUCER_COMPONENT documentType="Component" className="HeaderComponent" componentName=""
                 parentClasses="public juce::Component, private juce::Timer" constructorParams="std::shared_ptr&lt;PlayListScheduler&gt; playListScheduler"
                 variableInitialisers="" snapPixels="8" snapActive="1" snapShown="1"
                 overlayOpacity="0.330" fixedSize="0" initialWidth="1200" initialHeight="40">
  <BACKGROUND backgroundColour="ff2b2b2b"/>
  <TEXTBUTTON name="Link" id="4007ad9db8548718" memberName="link__textButton"
              virtualName="" explicitFocusOrder="0" pos="5 10 70 20" bgColOff="ff7a7a7a"
              bgColOn="ff12a4e2" buttonText="Link" connectedEdges="0" needsCallback="1"
              radioGroupId="0"/>
  <LABEL name="new label" id="8c4ed5ae2f97ba" memberName="tempo__label"
         virtualName="" explicitFocusOrder="0" pos="496 10 70 20" bkgCol="ff808080"
         edTextCol="ff000000" edBkgCol="0" labelText="120.0" editableSingleClick="1"
         editableDoubleClick="1" focusDiscardsChanges="0" fontname="Default font"
         fontsize="13.0" kerning="0.0" bold="0" italic="0" justification="36"/>
  <LABEL name="new label" id="7e154133c9506c7" memberName="bars__label"
         virtualName="" explicitFocusOrder="0" pos="235 10 70 20" bkgCol="ff808080"
         edTextCol="ff000000" edBkgCol="0" labelText="0" editableSingleClick="1"
         editableDoubleClick="1" focusDiscardsChanges="0" fontname="Default font"
         fontsize="13.0" kerning="0.0" bold="0" italic="0" justification="34"/>
  <LABEL name="new label" id="d5d8cb375330cb19" memberName="beats__label"
         virtualName="" explicitFocusOrder="0" pos="308 10 35 20" bkgCol="ff808080"
         edTextCol="ff000000" edBkgCol="0" labelText="0" editableSingleClick="1"
         editableDoubleClick="1" focusDiscardsChanges="0" fontname="Default font"
         fontsize="13.0" kerning="0.0" bold="0" italic="0" justification="34"/>
  <LABEL name="new label" id="95f57732373a9e8e" memberName="rest__label"
         virtualName="" explicitFocusOrder="0" pos="346 10 35 20" bkgCol="ff808080"
         edTextCol="ff000000" edBkgCol="0" labelText="0" editableSingleClick="1"
         editableDoubleClick="1" focusDiscardsChanges="0" fontname="Default font"
         fontsize="13.0" kerning="0.0" bold="0" italic="0" justification="34"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif


//[EndFile] You can add extra defines here...
//[/EndFile]

