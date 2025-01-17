
#include "HeaderComponent.h"

#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Link/LinkEngine.hpp"
#include "Interface/Controls/DefaultLabel.h"
#include "Interface/ColourIds.h"

using namespace juce;

//==============================================================================
HeaderComponent::HeaderComponent (std::shared_ptr<PlayListScheduler> playListScheduler) :
    playListScheduler(playListScheduler)
{
    // Link
    link__textButton.reset (new juce::TextButton ("Link"));
    addAndMakeVisible (link__textButton.get());
    link__textButton->addListener (this);
    link__textButton->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff7a7a7a));
    link__textButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff12a4e2));
    link__textButton->setClickingTogglesState(true);
    link__textButton->onClick = [this] {
        this->playListScheduler->getLinkEngine()->enableLink(link__textButton->getToggleState());
    };

    
    // Tempo
    tempoSlider = std::make_unique<juce::Slider>("Tempo Slider Font 13");
    addAndMakeVisible(tempoSlider.get());
    tempoSlider->setSliderStyle(juce::Slider::LinearBarVertical);
    tempoSlider->setVelocityModeParameters(1.0, 2, 0.001);
    tempoSlider->setVelocityBasedMode(true);
    tempoSlider->setDoubleClickReturnValue(true, 0.0);
    tempoSlider->setNormalisableRange(NormalisableRange<double>(30, 999, 0.01));
    tempoSlider->setColour(Slider::trackColourId, juce::Colours::transparentBlack);
    tempoSlider->setColour (Slider::backgroundColourId, juce::Colours::grey);
    tempoSlider->onValueChange = [this]() {
        this->playListScheduler->getTempoProvider()->setTempo(tempoSlider->getValue());
    };

    bars__label.reset (new DefaultLabel ("new label","0"));
    addAndMakeVisible (bars__label.get());
    bars__label->setEditable (false, false, false);
    bars__label->setJustificationType (juce::Justification::centredRight);
    bars__label->addListener (this);

    beats__label.reset (new DefaultLabel ("new label","0"));
    addAndMakeVisible (beats__label.get());
    beats__label->setJustificationType (juce::Justification::centredRight);
    beats__label->setEditable (false, false, false);
    beats__label->addListener (this);

    rest__label.reset (new DefaultLabel ("new label","0"));
    addAndMakeVisible (rest__label.get());
    rest__label->setJustificationType (juce::Justification::centredRight);
    rest__label->setEditable (false, false, false);
    rest__label->addListener (this);

    setSize (1200, 40);


    startTimerHz(60.f);
}

HeaderComponent::~HeaderComponent()
{
    stopTimer();
}

void HeaderComponent::paint (juce::Graphics& g)
{
    g.fillAll (findColour (audium::secondaryBackgroundColourId));
}

void HeaderComponent::resized()
{
    link__textButton->setBounds (5, 10, 70, 20);
    tempoSlider->setBounds (496, 10, 70, 20);
    bars__label->setBounds (235, 10, 70, 20);
    beats__label->setBounds (308, 10, 35, 20);
    rest__label->setBounds (346, 10, 35, 20);
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

    
    auto tempo = playListScheduler->getTempoProvider()->getTempo();
    tempoSlider->setValue(tempo, dontSendNotification);

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

