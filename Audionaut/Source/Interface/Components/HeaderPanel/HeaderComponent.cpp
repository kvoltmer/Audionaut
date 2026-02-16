//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "HeaderComponent.h"

#include "Engine/AudiumEngine.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/Link/LinkEngine.hpp"
#include "Engine/PlayList/TransportLoop.h"
#include "Engine/Recording/RecordingActionHandler.h"

#include "Interface/Controls/DefaultLabel.h"
#include "Interface/ColourIds.h"
#include "Interface/Components/MiddlePanel/ChannelView/ChannelComponent.h"

using namespace juce;

//==============================================================================
HeaderComponent::HeaderComponent (std::shared_ptr<audium::AudiumEngine> audiumEngine_) :
    audiumEngine(audiumEngine_)
{
    auto scheduler = audiumEngine->getPlayListScheduler().get();
    
    // Link
    linkButton.reset (new juce::TextButton ("Link"));
    addAndMakeVisible (linkButton.get());
    linkButton->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff7a7a7a));
    linkButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff12a4e2));
    linkButton->setClickingTogglesState(true);
    auto linkEngine = audiumEngine->getPlayListScheduler()->getLinkEngine().get();
    linkButton->onClick = [this, linkEngine] {
        linkEngine->enableLink(linkButton->getToggleState());
    };

    
    // Tempo
    tempoSlider = std::make_unique<juce::Slider>("Tempo Slider Font 13");
    addAndMakeVisible(tempoSlider.get());
    configureSlider(tempoSlider.get());
    tempoSlider->setVelocityModeParameters(1.0, 2, 0.001);
    tempoSlider->setNormalisableRange(NormalisableRange<double>(30, 999, 0.01));
    auto tempoProvider = audiumEngine->getPlayListScheduler()->getTempoProvider().get();
    tempoSlider->onValueChange = [this, tempoProvider]() {
        tempoProvider->setTempo(tempoSlider->getValue());
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
    
    barsSlider->onValueChange = [this, scheduler]() {
        auto barsDiff = static_cast<int>(barsSlider->getValue()) - lastBarsValue;
        if (abs(barsDiff) > 0) {
            auto clocksDiff = audium::TempoProvider::barsToClocks(barsDiff);
            auto clocks = scheduler->getAbsoluteStartPosition(audium::clocks);
            scheduler->setAbsoluteStartPosition(clocks + clocksDiff, audium::clocks);
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
    beatsSlider->onValueChange = [this, scheduler]() {
        auto beatsDiff = static_cast<int>(beatsSlider->getValue()) - lastBeatsValue;
        if (abs(beatsDiff) > 0) {
            auto clocksDiff = audium::TempoProvider::beatsToClocks(beatsDiff);
            auto clocks = scheduler->getAbsoluteStartPosition(audium::clocks);
            scheduler->setAbsoluteStartPosition(clocks + clocksDiff, audium::clocks);
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
    clicksSlider->onValueChange = [this, scheduler]() {
        auto clicksDiff = static_cast<int>(clicksSlider->getValue()) - lastClicksValue;
        if (abs(clicksDiff) > 0) {
            auto clocksDiff = audium::TempoProvider::clicksToClocks(clicksDiff);
            auto clocks = scheduler->getAbsoluteStartPosition(audium::clocks);
            scheduler->setAbsoluteStartPosition(clocks + clocksDiff, audium::clocks);
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
    
    // TODO: fix edit mode
    barsSlider->setTextBoxIsEditable(false);
    beatsSlider->setTextBoxIsEditable(false);
    clicksSlider->setTextBoxIsEditable(false);
    

    // PLAY
    playButton = std::make_unique<juce::DrawableButton>("Play", juce::DrawableButton::ButtonStyle::ImageOnButtonBackground);
    addAndMakeVisible(playButton.get());
    juce::Path play;
    play.addTriangle(0, 0, 0, 10, 10, 5);
    juce::DrawablePath playImage;
    playImage.setPath(play);
    playImage.setFill (FillType(Colours::white));
    playButton->setImages(&playImage);
    playButton->onClick = [this, scheduler]() {
        if (playButton->getToggleState()) {
            
            if (scheduler->isRecordingArmed()) {
                // Undo: store old state
                auto action = std::make_unique<audium::UndoableContainerAction>(*audiumEngine->getAudioTrackContainer(), false);
                scheduler->startRecording();
                // Undo: store new state
                action->storeNewState();
                audiumEngine->getAudioTrackContainer()->getUndoManager()->perform(action.release(), "Record");
                audiumEngine->getAudioTrackContainer()->getUndoManager()->beginNewTransaction();
            }
            scheduler->startPlaying();
        }
    };
    playButton->setClickingTogglesState(true);
    playButton->setColour (TextButton::buttonOnColourId, Colour (0xff12a4e2));
    playButton->setColour(TextButton::buttonColourId, Colours::grey);
    
    // STOP
    stopButton = std::make_unique<juce::DrawableButton>("Stop", juce::DrawableButton::ButtonStyle::ImageOnButtonBackground);
    addAndMakeVisible(stopButton.get());
    juce::Path stop;
    stop.addRoundedRectangle(0, 0, 10, 10, 2.f);
    juce::DrawablePath stopImage;
    stopImage.setPath(stop);
    stopImage.setFill (FillType(Colours::white));
    stopButton->setImages(&stopImage);
    stopButton->onClick = [this, scheduler]() {
        scheduler->stopPlaying();
    };
    stopButton->setColour(TextButton::buttonColourId, Colours::grey);
    
    // RECORD
    recordButton = std::make_unique<juce::DrawableButton>("Record", juce::DrawableButton::ButtonStyle::ImageOnButtonBackground);
    addAndMakeVisible(recordButton.get());
    juce::Path rec;
    rec.addEllipse(0, 0, 10, 10);
    juce::DrawablePath recImage;
    recImage.setPath(rec);
    recImage.setFill (FillType(Colours::red.darker().darker()));
    recordButton->setImages(&recImage);
    recordButton->onClick = [this, scheduler]() {
        scheduler->setRecordingArmed(recordButton->getToggleState());
        if (scheduler->isPlaying())
            scheduler->startRecording();
    };
    recordButton->setClickingTogglesState(true);
    recordButton->setColour (TextButton::buttonOnColourId, Colours::red.brighter().withAlpha(0.8f));
    recordButton->setColour(TextButton::buttonColourId, Colours::grey);
    
    
    // LOOP
    loopButton = std::make_unique<DrawableButton>("Loop",
                                                        DrawableButton::ButtonStyle::ImageOnButtonBackgroundOriginalSize);
    addAndMakeVisible(loopButton.get());
    juce::DrawablePath loopImage;
    loopImage.setPath(getLoopButtonPath());
    loopImage.setStrokeFill(FillType(Colours::white.withAlpha(0.8f)));
    loopImage.setStrokeThickness(1.5f);
    //loopImage.setFill (FillType(Colours::transparentBlack));
    loopImage.setFill (Colours::white);

    loopButton->setImages(&loopImage);
    loopButton->setClickingTogglesState(true);
    loopButton->setColour (TextButton::buttonOnColourId, Colour (0xff12a4e2));
    loopButton->setColour(TextButton::buttonColourId, Colours::grey);
    
    loopButton->onClick = [this, scheduler]() {
        scheduler->getTransportLoop()->setLoopActive(loopButton->getToggleState());
        audiumEngine->getAudioTrackContainer()->sendActionMessage(audium::updateArrangementAction);
    };
    
    
    // RIGHT PANEL
    rightPanelButton = std::make_unique<juce::ShapeButton>("Right Panel", Colours::transparentBlack, Colours::grey.withAlpha(0.25f), Colours::grey.withAlpha(0.25f));
    rightPanelButton->setOutline(Colours::white.withAlpha(0.75f), 1.2f);
    addAndMakeVisible(rightPanelButton.get());
    auto path = getRightPanelButtonPath();
    rightPanelButton->setShape(path, true, true, true);
    rightPanelButton->onClick = [this]() {
        NullCheckedInvocation::invoke(this->onRightPanelButtonClick);
    };
    
    
    // master meter
    stereoMeter = std::make_unique<StereoMeter>();
    addAndMakeVisible(stereoMeter.get());
    
    // master volume
    volumeSlider = std::make_unique<juce::Slider>("Master Volume Font 13");
    addAndMakeVisible(volumeSlider.get());
    ChannelComponent::configureVolumeSlider(volumeSlider.get());
    volumeSlider->setColour (Slider::backgroundColourId, juce::Colours::grey);

    volumeSlider->onValueChange = [this] {
        auto gain = Decibels::decibelsToGain(volumeSlider->getValue());
        this->audiumEngine->getAudioTrackContainer()->setMasterGain(gain);
    };
    volumeSlider->onDragStart = [this] {
        // TODO: undo/redo
    };
    
    volumeSlider->onDragEnd = [this] {
        // TODO: undo/redo
    };
    
    
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
    
    auto x = 400;
    stopButton->setBounds(x, 10, 35, 20);
    x += 38;
    playButton->setBounds(x, 10, 35, 20);
    x += 38;
    recordButton->setBounds(x, 10, 35, 20);
    x += 50;
    loopButton->setBounds(x, 10, 35, 20);
    
    x += 100;
    volumeSlider->setBounds(x, 10, 70, 20);
    x += 100;
    stereoMeter->setBounds(x, 10, 110, 20);
    
    rightPanelButton->setBounds(getWidth() - 40, 10, 30, 20);
}

void HeaderComponent::updateUI()
{
    auto gain = audiumEngine->getAudioTrackContainer()->getMasterGain();
    volumeSlider->setValue(LevelMeter::gainToDecebel(gain), dontSendNotification);
    auto loopActive = audiumEngine->getPlayListScheduler()->getTransportLoop()->isLoopActive();
    loopButton->setToggleState(loopActive, dontSendNotification);
    
}

void HeaderComponent::timerCallback()
{
    auto scheduler = audiumEngine->getPlayListScheduler().get();
    
    const auto numPeers = scheduler->getLinkEngine()->numPeers();
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

    
    auto tempo = scheduler->getTempoProvider()->getTempo();
    tempoSlider->setValue(tempo, dontSendNotification);

    
    auto positionInClocks = scheduler->getAbsolutePosition(audium::clocks);
    
    auto bars = std::floor(audium::TempoProvider::clocksToBars(positionInClocks));
    barsSlider->setValue(bars, juce::dontSendNotification);
    
    positionInClocks -= audium::TempoProvider::barsToClocks(bars);
    auto beats = std::floor(audium::TempoProvider::clocksToBeats(positionInClocks));
    beatsSlider->setValue(beats, juce::dontSendNotification);
                            
    positionInClocks -= audium::TempoProvider::beatsToClocks(beats);
    auto clicks = std::floor(audium::TempoProvider::clocksToClicks(positionInClocks));
    clicksSlider->setValue(clicks, juce::dontSendNotification);
    
    for (auto c = 0; c < 2; ++c)
        stereoMeter->setLevel(c, scheduler->getAudioBusInterface()->getMasterLevel(c));
    
    playButton->setToggleState(audiumEngine->getPlayListScheduler()->isPlaying(),
                               dontSendNotification);
    
    recordButton->setToggleState(audiumEngine->getPlayListScheduler()->isRecordingArmed(),
                                 dontSendNotification);
    
    if (audiumEngine->getPlayListScheduler()->isRecording()) {
        audiumEngine->getRecordingActionHandler()->onTimerUpdate();
    }
}

void HeaderComponent::configureSlider(juce::Slider* slider)
{
    slider->setSliderStyle(juce::Slider::LinearBarVertical);
    slider->setVelocityBasedMode(true);
    slider->setDoubleClickReturnValue(true, 0.0);
    slider->setColour(Slider::trackColourId, juce::Colours::transparentBlack);
    slider->setColour (Slider::backgroundColourId, juce::Colours::grey);
}

juce::Path HeaderComponent::getRightPanelButtonPath()
{
    juce::Path rightPanelPath;
    auto w = 30;
    auto h = 20;
    auto b = 19;
    
    rightPanelPath.addRoundedRectangle(0, 0, w, h, 2.f);
    rightPanelPath.startNewSubPath(b, 0);
    rightPanelPath.lineTo(b, h);
    rightPanelPath.closeSubPath();
    
    
    
    auto y = 5.f;
    auto s = 3.f;
    for (auto i = 0; i < 3; i++) {
        rightPanelPath.startNewSubPath(b+s, y);
        rightPanelPath.lineTo(w-s, y);
        rightPanelPath.closeSubPath();
        y += 4.f;
    }
    
    return rightPanelPath;
}

juce::Path HeaderComponent::getLoopButtonPath()
{
    auto w = 20.f;
    auto h = 10.f;
    auto gap = 5.f;
    juce::Path loop;
    
    loop.startNewSubPath ({gap, h});
    loop.lineTo(0.f, h);
    loop.lineTo(0.f, 0.f);
    loop.lineTo(w, 0.f);
    loop.lineTo(w, h);
    loop.lineTo(w - gap, h);
    
    // go back
    loop.lineTo(w, h);
    loop.lineTo(w, 0.f);
    loop.lineTo(0.f, 0.f);
    loop.lineTo(0.f, h);
    
    loop = loop.createPathWithRoundedCorners(4.f);
    
    // arrow
    auto x = w-gap;
    auto y = h;
    auto arrowW = 5.f;
    auto arrowH = 2.f;
    loop.addTriangle(x-arrowW, y, x, y - arrowH, x, y + arrowH);
    
    return loop;
}
