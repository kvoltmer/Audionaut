
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Link/LinkEngine.hpp"


#include "HeaderComponent.h"
#include "Interface/Controls/DefaultLabel.h"


//==============================================================================
HeaderComponent::HeaderComponent (std::shared_ptr<PlayListScheduler> playListScheduler) :
    playListScheduler(playListScheduler)
{
    link__textButton.reset (new juce::TextButton ("Link"));
    addAndMakeVisible (link__textButton.get());
    link__textButton->addListener (this);
    link__textButton->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff7a7a7a));
    link__textButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff12a4e2));

    link__textButton->setBounds (5, 10, 70, 20);

    tempoLabel.reset (new DefaultLabel ("tempo", "120.0"));
    addAndMakeVisible (tempoLabel.get());
    tempoLabel->addListener (this);
    tempoLabel->setBounds (496, 10, 70, 20);

    bars__label.reset (new DefaultLabel ("new label","0"));
    addAndMakeVisible (bars__label.get());
    bars__label->setEditable (false, false, false);
    bars__label->setJustificationType (juce::Justification::centredRight);
    bars__label->addListener (this);
    bars__label->setBounds (235, 10, 70, 20);

    beats__label.reset (new DefaultLabel ("new label","0"));
    addAndMakeVisible (beats__label.get());
    beats__label->setJustificationType (juce::Justification::centredRight);
    beats__label->setEditable (false, false, false);
    beats__label->addListener (this);
    beats__label->setBounds (308, 10, 35, 20);

    rest__label.reset (new DefaultLabel ("new label","0"));
    addAndMakeVisible (rest__label.get());
    rest__label->setJustificationType (juce::Justification::centredRight);
    rest__label->setEditable (false, false, false);
    rest__label->addListener (this);
    rest__label->setBounds (346, 10, 35, 20);

    setSize (1200, 40);



    link__textButton->onClick = [this] { this->playListScheduler->getLinkEngine()->enableLink(link__textButton->getToggleState()); };

    link__textButton->setClickingTogglesState(true);

    startTimerHz(60.f);

}

HeaderComponent::~HeaderComponent()
{
    link__textButton = nullptr;
    tempoLabel = nullptr;
    bars__label = nullptr;
    beats__label = nullptr;
    rest__label = nullptr;

}

void HeaderComponent::paint (juce::Graphics& g)
{
}

void HeaderComponent::resized()
{
}

void HeaderComponent::buttonClicked (juce::Button* buttonThatWasClicked)
{
    if (buttonThatWasClicked == link__textButton.get())
    {
        auto state = link__textButton->getToggleStateValue();
    }
}

void HeaderComponent::labelTextChanged (juce::Label* labelThatHasChanged)
{
    if (labelThatHasChanged == tempoLabel.get())
    {
        playListScheduler->getTempoProvider()->setTempo(tempoLabel->getText().getDoubleValue());
    }
    else if (labelThatHasChanged == bars__label.get())
    {
    }
    else if (labelThatHasChanged == beats__label.get())
    {
    }
    else if (labelThatHasChanged == rest__label.get())
    {
    }
}

void HeaderComponent::timerCallback()
{
    const auto numPeers = playListScheduler->getLinkEngine()->numPeers();
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

    if (not tempoLabel->isBeingEdited())
    {
        const auto tempo = playListScheduler->getTempoProvider()->getTempo();
        tempoLabel->setText(juce::String(tempo, 2), juce::dontSendNotification);
    }

    //auto beats = playListScheduler->getLinkEngine()->beatTime();
    const auto clocks = playListScheduler->getAbsolutePosition(audium::clocks);

    const auto beats = TempoProvider::clocksToBeats(clocks);
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
         outlineCol="ff404040" edTextCol="ff000000" edBkgCol="0" hiliteCol="bff4ff80"
         labelText="120.0" editableSingleClick="1" editableDoubleClick="1"
         focusDiscardsChanges="0" fontname="Default font" fontsize="13.0"
         kerning="0.0" bold="0" italic="0" justification="36"/>
  <LABEL name="new label" id="7e154133c9506c7" memberName="bars__label"
         virtualName="" explicitFocusOrder="0" pos="235 10 70 20" bkgCol="ff808080"
         edTextCol="ff000000" edBkgCol="0" hiliteCol="fffafa93" labelText="0"
         editableSingleClick="1" editableDoubleClick="1" focusDiscardsChanges="0"
         fontname="Default font" fontsize="13.0" kerning="0.0" bold="0"
         italic="0" justification="34"/>
  <LABEL name="new label" id="d5d8cb375330cb19" memberName="beats__label"
         virtualName="" explicitFocusOrder="0" pos="308 10 35 20" bkgCol="ff808080"
         edTextCol="ff000000" edBkgCol="0" hiliteCol="bfe9f37e" labelText="0"
         editableSingleClick="1" editableDoubleClick="1" focusDiscardsChanges="0"
         fontname="Default font" fontsize="13.0" kerning="0.0" bold="0"
         italic="0" justification="34"/>
  <LABEL name="new label" id="95f57732373a9e8e" memberName="rest__label"
         virtualName="" explicitFocusOrder="0" pos="346 10 35 20" bkgCol="ff808080"
         edTextCol="ff000000" edBkgCol="0" hiliteCol="bfe9f574" labelText="0"
         editableSingleClick="1" editableDoubleClick="1" focusDiscardsChanges="0"
         fontname="Default font" fontsize="13.0" kerning="0.0" bold="0"
         italic="0" justification="34"/>
</JUCER_COMPONENT>

END_JUCER_METADATA
*/
#endif


//[EndFile] You can add extra defines here...
//[/EndFile]

