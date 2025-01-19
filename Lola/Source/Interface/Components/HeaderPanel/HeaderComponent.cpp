
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
    linkButton.reset (new juce::TextButton ("Link"));
    addAndMakeVisible (linkButton.get());
    linkButton->addListener (this);
    linkButton->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff7a7a7a));
    linkButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff12a4e2));
    linkButton->setClickingTogglesState(true);
    linkButton->onClick = [this] {
        this->playListScheduler->getLinkEngine()->enableLink(linkButton->getToggleState());
    };

    
    // Tempo
    tempoSlider = std::make_unique<juce::Slider>("Tempo Slider Font 13");
    addAndMakeVisible(tempoSlider.get());
    configureSlider(tempoSlider.get());
    tempoSlider->setVelocityModeParameters(1.0, 2, 0.001);
    tempoSlider->setNormalisableRange(NormalisableRange<double>(30, 999, 0.01));
    tempoSlider->onValueChange = [this]() {
        this->playListScheduler->getTempoProvider()->setTempo(tempoSlider->getValue());
    };

    // Bars
    barsSlider = std::make_unique<juce::Slider>("Bars Slider Font 13");
    addAndMakeVisible (barsSlider.get());
    configureSlider(barsSlider.get());
    barsSlider->setVelocityModeParameters(1.0, 1, 0.01);
    barsSlider->setNormalisableRange(NormalisableRange<double>(0, 1000, 1));
    barsSlider->onDragStart = [this]() {
        lastBarsValue = static_cast<int>(barsSlider->getValue());
    };
    barsSlider->onValueChange = [this]() {
        auto barsDiff = static_cast<int>(barsSlider->getValue()) - lastBarsValue;
        if (abs(barsDiff) > 0) {
            auto clocksDiff = TempoProvider::barsToClocks(barsDiff);
            auto clocks = this->playListScheduler->getAbsoluteStartPosition(audium::clocks);
            this->playListScheduler->setAbsoluteStartPosition(clocks + clocksDiff, audium::clocks);
            lastBarsValue = static_cast<int>(barsSlider->getValue());
        }
    };
    barsSlider->textFromValueFunction = [](auto val) {
        return String(static_cast<int>(val) + 1);
    };
    barsSlider->valueFromTextFunction = [] (auto string) {
        return string.getDoubleValue() - 1.0;
    };
    barsSlider->updateText();
    
    
    // Beats
    beatsSlider = std::make_unique<juce::Slider>("Beats Slider Font 13");
    addAndMakeVisible (beatsSlider.get());
    configureSlider(beatsSlider.get());
    beatsSlider->setVelocityModeParameters(1.0, 1, 0.01);
    beatsSlider->setNormalisableRange(NormalisableRange<double>(0, 4000, 1));
    beatsSlider->onDragStart = [this]() {
        lastBeatsValue = static_cast<int>(beatsSlider->getValue());
    };
    beatsSlider->onValueChange = [this]() {
        auto beatsDiff = static_cast<int>(beatsSlider->getValue()) - lastBeatsValue;
        if (abs(beatsDiff) > 0) {
            auto clocksDiff = TempoProvider::beatsToClocks(beatsDiff);
            auto clocks = this->playListScheduler->getAbsoluteStartPosition(audium::clocks);
            this->playListScheduler->setAbsoluteStartPosition(clocks + clocksDiff, audium::clocks);
            lastBeatsValue = static_cast<int>(beatsSlider->getValue());
        }
    };
    
    beatsSlider->textFromValueFunction = [](auto val) {
        return String((static_cast<int>(val) % 4) + 1);
    };
    beatsSlider->valueFromTextFunction = [] (auto string) {
        return string.getDoubleValue() - 1.0;
    };
    beatsSlider->updateText();
    

    // Clicks
    clicksSlider = std::make_unique<juce::Slider>("Clocks Slider Font 13");
    addAndMakeVisible (clicksSlider.get());
    configureSlider(clicksSlider.get());
    clicksSlider->setVelocityModeParameters(1.0, 1, 0.01);
    clicksSlider->setNormalisableRange(NormalisableRange<double>(0, 16000, 1));
    beatsSlider->onDragStart = [this]() {
        lastClicksValue = static_cast<int>(clicksSlider->getValue());
    };
    clicksSlider->onValueChange = [this]() {
        auto clicksDiff = static_cast<int>(clicksSlider->getValue()) - lastClicksValue;
        if (abs(clicksDiff) > 0) {
            auto clocksDiff = TempoProvider::clicksToClocks(clicksDiff);
            auto clocks = this->playListScheduler->getAbsoluteStartPosition(audium::clocks);
            this->playListScheduler->setAbsoluteStartPosition(clocks + clocksDiff, audium::clocks);
            lastClicksValue = static_cast<int>(clicksSlider->getValue());
        }
    };
    clicksSlider->textFromValueFunction = [](auto val) {
        auto intVal = (static_cast<int>(val) % 4) + 1;
        return String(intVal);
    };
    clicksSlider->valueFromTextFunction = [] (auto string) {
        return string.getDoubleValue() - 1.0;
    };
    clicksSlider->updateText();
    

    // PLAY
    playButton = std::make_unique<juce::DrawableButton>("Play", juce::DrawableButton::ButtonStyle::ImageOnButtonBackground);
    addAndMakeVisible(playButton.get());
    juce::Path play;
    play.addTriangle(0, 0, 0, 10, 10, 5);
    playImage.setPath(play);
    playImage.setFill (FillType(Colours::white));
    playButton->setImages(&playImage);
    playButton->onClick = [this]() {
        this->playListScheduler->startPlaying();
    };
    playButton->setColour(TextButton::buttonColourId, Colours::grey);
    
    // STOP
    stopButton = std::make_unique<juce::DrawableButton>("Stop", juce::DrawableButton::ButtonStyle::ImageOnButtonBackground);
    addAndMakeVisible(stopButton.get());
    juce::Path stop;
    stop.addRoundedRectangle(0, 0, 10, 10, 2.f);
    stopImage.setPath(stop);
    stopImage.setFill (FillType(Colours::white));
    stopButton->setImages(&stopImage);
    stopButton->onClick = [this]() {
        this->playListScheduler->stopPlaying();
    };
    stopButton->setColour(TextButton::buttonColourId, Colours::grey);
    
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
    linkButton->setBounds (5, 10, 70, 20);
    tempoSlider->setBounds (125, 10, 70, 20);
    
    barsSlider->setBounds (235, 10, 70, 20);
    beatsSlider->setBounds (308, 10, 35, 20);
    clicksSlider->setBounds (346, 10, 35, 20);
    
    playButton->setBounds(450, 10, 35, 20);
    stopButton->setBounds(500, 10, 35, 20);
}

void HeaderComponent::buttonClicked (juce::Button* buttonThatWasClicked)
{
    if (buttonThatWasClicked == linkButton.get())
    {
        auto state = linkButton->getToggleStateValue();
    }
}

void HeaderComponent::labelTextChanged (juce::Label* labelThatHasChanged)
{
}

void HeaderComponent::timerCallback()
{
    const auto numPeers = playListScheduler->getLinkEngine()->numPeers();
    juce::String txt;
    if (numPeers > 0) {
        txt = juce::String(numPeers) + " Link";
        if (numPeers > 1)
            txt += "s";
    }
    else {
        txt = "Link";
    }
    linkButton->setButtonText(txt);

    
    auto tempo = playListScheduler->getTempoProvider()->getTempo();
    tempoSlider->setValue(tempo, dontSendNotification);

    
    auto clocks = playListScheduler->getAbsolutePosition(audium::clocks);
    barsSlider->setValue(TempoProvider::clocksToBars(clocks), juce::dontSendNotification);
    beatsSlider->setValue(TempoProvider::clocksToBeats(clocks), juce::dontSendNotification);
    clicksSlider->setValue(TempoProvider::clocksToClicks(clocks), juce::dontSendNotification);
}

void HeaderComponent::configureSlider(juce::Slider* slider)
{
    slider->setSliderStyle(juce::Slider::LinearBarVertical);
    slider->setVelocityBasedMode(true);
    slider->setDoubleClickReturnValue(true, 0.0);
    slider->setColour(Slider::trackColourId, juce::Colours::transparentBlack);
    slider->setColour (Slider::backgroundColourId, juce::Colours::grey);
}
