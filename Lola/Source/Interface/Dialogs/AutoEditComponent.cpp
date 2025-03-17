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


#include "AutoEditComponent.h"



//==============================================================================
AutoEditComponent::AutoEditComponent ()
{


    juce__label.reset (new juce::Label ("new label",
                                        TRANS ("Assemble Duration (Seconds)")));
    addAndMakeVisible (juce__label.get());
    juce__label->setFont (juce::FontOptions (12.00f));
    juce__label->setJustificationType (juce::Justification::centredLeft);
    juce__label->setEditable (false, false, false);
    juce__label->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    juce__label->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));

    juce__label->setBounds (0, 39, 150, 24);

    juce__label2.reset (new juce::Label ("new label",
                                         TRANS ("Number of Segments")));
    addAndMakeVisible (juce__label2.get());
    juce__label2->setFont (juce::FontOptions (12.00f));
    juce__label2->setJustificationType (juce::Justification::centredLeft);
    juce__label2->setEditable (false, false, false);
    juce__label2->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    juce__label2->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));

    juce__label2->setBounds (0, 73, 150, 24);

    juce__label3.reset (new juce::Label ("new label",
                                         TRANS ("Min. Segment Length")));
    addAndMakeVisible (juce__label3.get());
    juce__label3->setFont (juce::FontOptions (12.00f));
    juce__label3->setJustificationType (juce::Justification::centredLeft);
    juce__label3->setEditable (false, false, false);
    juce__label3->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    juce__label3->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));

    juce__label3->setBounds (0, 107, 150, 24);

    juce__label4.reset (new juce::Label ("new label",
                                         TRANS ("Max. Segment Length")));
    addAndMakeVisible (juce__label4.get());
    juce__label4->setFont (juce::FontOptions (12.00f));
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
    juce__label5->setFont (juce::FontOptions (12.00f));
    juce__label5->setJustificationType (juce::Justification::centredLeft);
    juce__label5->setEditable (false, false, false);
    juce__label5->setColour (juce::TextEditor::textColourId, juce::Colours::black);
    juce__label5->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));

    juce__label5->setBounds (0, 5, 150, 24);


    setSize (305, 170);


}

AutoEditComponent::~AutoEditComponent()
{
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

}

void AutoEditComponent::paint (juce::Graphics& g)
{
}

void AutoEditComponent::resized()
{
}

void AutoEditComponent::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == mode.get())
    {

    }

}


