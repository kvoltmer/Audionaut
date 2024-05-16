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
//[/Headers]

#include "AutoEditComponent.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
AutoEditComponent::AutoEditComponent ()
{
    //[Constructor_pre] You can add your own custom stuff here..
    //[/Constructor_pre]

    juce__label.reset (new juce::Label ("new label",
                                        TRANS ("Assemble Duration")));
    addAndMakeVisible (juce__label.get());
    juce__label->setFont (juce::Font (15.00f, juce::Font::plain).withTypefaceStyle ("Regular"));
    juce__label->setJustificationType (juce::Justification::centredLeft);
    juce__label->setEditable (false, false, false);
    juce__label->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    juce__label->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));

    juce__label->setBounds (0, 39, 150, 24);

    juce__label2.reset (new juce::Label ("new label",
                                         TRANS ("Number of Segments")));
    addAndMakeVisible (juce__label2.get());
    juce__label2->setFont (juce::Font (15.00f, juce::Font::plain).withTypefaceStyle ("Regular"));
    juce__label2->setJustificationType (juce::Justification::centredLeft);
    juce__label2->setEditable (false, false, false);
    juce__label2->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    juce__label2->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));

    juce__label2->setBounds (0, 73, 150, 24);

    juce__label3.reset (new juce::Label ("new label",
                                         TRANS ("Min. Segment Length")));
    addAndMakeVisible (juce__label3.get());
    juce__label3->setFont (juce::Font (15.00f, juce::Font::plain).withTypefaceStyle ("Regular"));
    juce__label3->setJustificationType (juce::Justification::centredLeft);
    juce__label3->setEditable (false, false, false);
    juce__label3->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    juce__label3->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));

    juce__label3->setBounds (0, 107, 150, 24);

    juce__label4.reset (new juce::Label ("new label",
                                         TRANS ("Max. Segment Length")));
    addAndMakeVisible (juce__label4.get());
    juce__label4->setFont (juce::Font (15.00f, juce::Font::plain).withTypefaceStyle ("Regular"));
    juce__label4->setJustificationType (juce::Justification::centredLeft);
    juce__label4->setEditable (false, false, false);
    juce__label4->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    juce__label4->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));

    juce__label4->setBounds (0, 141, 150, 24);

    duration.reset (new juce::TextEditor ("duration"));
    addAndMakeVisible (duration.get());
    duration->setMultiLine (false);
    duration->setReturnKeyStartsNewLine (false);
    duration->setReadOnly (false);
    duration->setScrollbarsShown (true);
    duration->setCaretVisible (true);
    duration->setPopupMenuEnabled (true);
    duration->setText (TRANS ("180"));

    duration->setBounds (149, 39, 150, 24);

    numSegments.reset (new juce::TextEditor ("new text editor"));
    addAndMakeVisible (numSegments.get());
    numSegments->setMultiLine (false);
    numSegments->setReturnKeyStartsNewLine (false);
    numSegments->setReadOnly (false);
    numSegments->setScrollbarsShown (true);
    numSegments->setCaretVisible (true);
    numSegments->setPopupMenuEnabled (true);
    numSegments->setText (TRANS ("20"));

    numSegments->setBounds (149, 73, 150, 24);

    segmentMin.reset (new juce::TextEditor ("new text editor"));
    addAndMakeVisible (segmentMin.get());
    segmentMin->setMultiLine (false);
    segmentMin->setReturnKeyStartsNewLine (false);
    segmentMin->setReadOnly (true);
    segmentMin->setScrollbarsShown (true);
    segmentMin->setCaretVisible (false);
    segmentMin->setPopupMenuEnabled (true);
    segmentMin->setText (TRANS ("2.0"));

    segmentMin->setBounds (149, 107, 150, 24);

    segmentMax.reset (new juce::TextEditor ("new text editor"));
    addAndMakeVisible (segmentMax.get());
    segmentMax->setMultiLine (false);
    segmentMax->setReturnKeyStartsNewLine (false);
    segmentMax->setReadOnly (true);
    segmentMax->setScrollbarsShown (true);
    segmentMax->setCaretVisible (false);
    segmentMax->setPopupMenuEnabled (true);
    segmentMax->setText (TRANS ("60.0"));

    segmentMax->setBounds (149, 141, 150, 24);

    mode.reset (new juce::ComboBox ("new combo box"));
    addAndMakeVisible (mode.get());
    mode->setEditableText (false);
    mode->setJustificationType (juce::Justification::centredLeft);
    mode->setTextWhenNothingSelected (TRANS ("Random"));
    mode->setTextWhenNoChoicesAvailable (TRANS ("(no choices)"));
    mode->addItem (TRANS ("Random"), 1);
    mode->addItem (TRANS ("Sequential"), 2);
    mode->addSeparator();
    mode->addListener (this);

    mode->setBounds (150, 5, 150, 24);

    juce__label5.reset (new juce::Label ("new label",
                                         TRANS ("Assemble Mode")));
    addAndMakeVisible (juce__label5.get());
    juce__label5->setFont (juce::Font (15.00f, juce::Font::plain).withTypefaceStyle ("Regular"));
    juce__label5->setJustificationType (juce::Justification::centredLeft);
    juce__label5->setEditable (false, false, false);
    juce__label5->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    juce__label5->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));

    juce__label5->setBounds (0, 5, 150, 24);


    //[UserPreSize]
    //[/UserPreSize]

    setSize (305, 170);


    //[Constructor] You can add your own custom stuff here..
    //[/Constructor]
}

AutoEditComponent::~AutoEditComponent()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]

    juce__label = nullptr;
    juce__label2 = nullptr;
    juce__label3 = nullptr;
    juce__label4 = nullptr;
    duration = nullptr;
    numSegments = nullptr;
    segmentMin = nullptr;
    segmentMax = nullptr;
    mode = nullptr;
    juce__label5 = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void AutoEditComponent::paint (juce::Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (juce::Colour (0xff323e44));

    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void AutoEditComponent::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    //[UserResized] Add your own custom resize handling here..
    //[/UserResized]
}

void AutoEditComponent::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    //[UsercomboBoxChanged_Pre]
    //[/UsercomboBoxChanged_Pre]

    if (comboBoxThatHasChanged == mode.get())
    {
        //[UserComboBoxCode_mode] -- add your combo box handling code here..
        //[/UserComboBoxCode_mode]
    }

    //[UsercomboBoxChanged_Post]
    //[/UsercomboBoxChanged_Post]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...
//[/MiscUserCode]


//==============================================================================
#if 0
/*  -- Projucer information section --

    This is where the Projucer stores the metadata that describe this GUI layout, so
    make changes in here at your peril!

BEGIN_JUCER_METADATA

<JUCER_COMPONENT documentType="Component" className="AutoEditComponent" componentName=""
                 parentClasses="public juce::Component" constructorParams="" variableInitialisers=""
                 snapPixels="8" snapActive="1" snapShown="1" overlayOpacity="0.330"
                 fixedSize="0" initialWidth="305" initialHeight="170">
  <BACKGROUND backgroundColour="ff323e44"/>
  <LABEL name="new label" id="a73452a0976e835a" memberName="juce__label"
         virtualName="" explicitFocusOrder="0" pos="0 39 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="Assemble Duration" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
         fontsize="15.0" kerning="0.0" bold="0" italic="0" justification="33"/>
  <LABEL name="new label" id="6bc485c36516d34f" memberName="juce__label2"
         virtualName="" explicitFocusOrder="0" pos="0 73 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="Number of Segments" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
         fontsize="15.0" kerning="0.0" bold="0" italic="0" justification="33"/>
  <LABEL name="new label" id="583f3ad8314ea0e2" memberName="juce__label3"
         virtualName="" explicitFocusOrder="0" pos="0 107 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="Min. Segment Length" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
         fontsize="15.0" kerning="0.0" bold="0" italic="0" justification="33"/>
  <LABEL name="new label" id="b0e5864f96bdc71e" memberName="juce__label4"
         virtualName="" explicitFocusOrder="0" pos="0 141 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="Max. Segment Length" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
         fontsize="15.0" kerning="0.0" bold="0" italic="0" justification="33"/>
  <TEXTEDITOR name="duration" id="b471f82b6e65e3fa" memberName="duration" virtualName=""
              explicitFocusOrder="0" pos="149 39 150 24" initialText="180"
              multiline="0" retKeyStartsLine="0" readonly="0" scrollbars="1"
              caret="1" popupmenu="1"/>
  <TEXTEDITOR name="new text editor" id="5e98bc81b03d3f15" memberName="numSegments"
              virtualName="" explicitFocusOrder="0" pos="149 73 150 24" initialText="20"
              multiline="0" retKeyStartsLine="0" readonly="0" scrollbars="1"
              caret="1" popupmenu="1"/>
  <TEXTEDITOR name="new text editor" id="da24932cb30eca0a" memberName="segmentMin"
              virtualName="" explicitFocusOrder="0" pos="149 107 150 24" initialText="2.0"
              multiline="0" retKeyStartsLine="0" readonly="1" scrollbars="1"
              caret="0" popupmenu="1"/>
  <TEXTEDITOR name="new text editor" id="875d6e05d72f63e1" memberName="segmentMax"
              virtualName="" explicitFocusOrder="0" pos="149 141 150 24" initialText="60.0"
              multiline="0" retKeyStartsLine="0" readonly="1" scrollbars="1"
              caret="0" popupmenu="1"/>
  <COMBOBOX name="new combo box" id="fd2cbbc090aecc0" memberName="mode" virtualName=""
            explicitFocusOrder="0" pos="150 5 150 24" editable="0" layout="33"
            items="Random&#10;Sequential&#10;" textWhenNonSelected="Random"
            textWhenNoItems="(no choices)"/>
  <LABEL name="new label" id="825a8c9d0b9c9ab4" memberName="juce__label5"
         virtualName="" explicitFocusOrder="0" pos="0 5 150 24" edTextCol="ff000000"
         edBkgCol="0" labelText="Assemble Mode" editableSingleClick="0"
         editableDoubleClick="0" focusDiscardsChanges="0" fontname="Default font"
         fontsize="15.0" kerning="0.0" bold="0" italic="0" justification="33"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif


//[EndFile] You can add extra defines here...
//[/EndFile]

