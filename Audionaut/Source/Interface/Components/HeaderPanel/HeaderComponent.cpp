//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "HeaderComponent.h"

#include "Engine/AudiumEngine.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/Link/LinkEngine.hpp"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/PlayList/TransportLoop.h"
#include "Engine/Recording/RecordingActionHandler.h"
#include "Engine/Undo/UndoableEdit.h"

#include "Interface/Controls/DefaultLabel.h"
#include "Interface/ColourIds.h"
#include "Interface/Components/MiddlePanel/ChannelView/ChannelComponent.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

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
    linkButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour (audium::toggledOnColour));
    linkButton->setClickingTogglesState(true);
    auto linkEngine = audiumEngine->getPlayListScheduler()->getLinkEngine().get();
    linkButton->onClick = [this, linkEngine] {
        linkEngine->enableLink(linkButton->getToggleState());
    };

    
    // Tempo
    tempoSlider = std::make_unique<juce::Slider>("Tempo Slider Font 13");
    addAndMakeVisible(tempoSlider.get());
    AudiumLookAndFeel::configureSlider(tempoSlider.get());
    tempoSlider->setVelocityModeParameters(1.0, 2, 0.001);
    tempoSlider->setNormalisableRange(NormalisableRange<double>(30, 999, 0.01));
    auto tempoProvider = audiumEngine->getPlayListScheduler()->getTempoProvider().get();
    tempoSlider->onValueChange = [this, tempoProvider]() {
        tempoProvider->setTempo(tempoSlider->getValue());
    };

    // Position: bars, beats and clicks of the play position
    barsSlider = makePositionSlider ("Bars Slider Font 13", 1000, lastBarsValue,
                                     audium::TempoProvider::barsToClocks,
                                     [] (double val) { return String (static_cast<int> (val) + 1); });
    beatsSlider = makePositionSlider ("Beats Slider Font 13", 4000, lastBeatsValue,
                                      audium::TempoProvider::beatsToClocks,
                                      [] (double val) { return String ((static_cast<int> (val) % 4) + 1); });
    clicksSlider = makePositionSlider ("Clocks Slider Font 13", 16000, lastClicksValue,
                                       audium::TempoProvider::clicksToClocks,
                                       [] (double val) { return String ((static_cast<int> (val) % 4) + 1); });
    

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
                audium::applyAsUndoableEdit(*audiumEngine->getAudioTrackContainer(),
                                            [&scheduler] { scheduler->startRecording(); return true; },
                                            "Record", false);
            }
            scheduler->startPlaying();
        }
    };
    playButton->setClickingTogglesState(true);
    playButton->setColour (TextButton::buttonOnColourId, Colour (audium::toggledOnColour));
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
    loopImage.setFill (Colours::white);

    loopButton->setImages(&loopImage);
    loopButton->setClickingTogglesState(true);
    loopButton->setColour (TextButton::buttonOnColourId, Colour (audium::toggledOnColour));
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

    // dsp load meter
    loadMeter = std::make_unique<LoadMeter>();
    addAndMakeVisible(loadMeter.get());
    
    // master volume
    volumeSlider = std::make_unique<juce::Slider>("Master Volume Font 13");
    addAndMakeVisible(volumeSlider.get());
    AudiumLookAndFeel::configureVolumeSlider(volumeSlider.get());
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
    startTimerHz(AudiumLookAndFeel::timerHz);
}

std::unique_ptr<juce::Slider> HeaderComponent::makePositionSlider (const juce::String& name,
                                                                   double maximum,
                                                                   int& lastValue,
                                                                   double (*unitsToClocks) (double),
                                                                   std::function<juce::String (double)> textFromValue)
{
    auto slider = std::make_unique<juce::Slider> (name);
    auto* s = slider.get();
    auto scheduler = audiumEngine->getPlayListScheduler().get();

    addAndMakeVisible (s);
    AudiumLookAndFeel::configureSlider (s);
    s->setVelocityModeParameters (1.0, 1, 0.01);
    s->setNormalisableRange (NormalisableRange<double> (0, maximum, 1));

    // The timer moves the value without notification, so remember where a
    // drag starts and move the play position by the dragged difference.
    s->onDragStart = [s, &lastValue]() {
        lastValue = static_cast<int> (s->getValue());
    };
    s->onValueChange = [s, &lastValue, scheduler, unitsToClocks]() {
        auto diff = static_cast<int> (s->getValue()) - lastValue;
        if (abs (diff) > 0) {
            auto clocksDiff = unitsToClocks (diff);
            auto clocks = scheduler->getAbsoluteStartPosition (audium::clocks);
            scheduler->setAbsoluteStartPosition (clocks + clocksDiff, audium::clocks);
            lastValue = static_cast<int> (s->getValue());
        }
    };
    s->textFromValueFunction = std::move (textFromValue);
    s->valueFromTextFunction = [] (auto string) {
        return string.getDoubleValue() - 1.0;
    };
    s->updateText();

    // TODO: fix edit mode
    s->setTextBoxIsEditable (false);

    return slider;
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
    x += 116;
    loadMeter->setBounds(x, 10, 10, 20);

    rightPanelButton->setBounds(getWidth() - 40, 10, 30, 20);
}

void HeaderComponent::updateUI()
{
    auto gain = audiumEngine->getAudioTrackContainer()->getMasterGain();
    volumeSlider->setValue(LevelMeter::gainToDecebel(gain), dontSendNotification);
    auto loopActive = audiumEngine->getPlayListScheduler()->getTransportLoop()->isLoopActive();
    loopButton->setToggleState(loopActive, dontSendNotification);
    
    linkButton->setToggleState(audiumEngine->getPlayListScheduler()->getLinkEngine()->isEnabled(), dontSendNotification);
    
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

    if (auto linkAudioDevice = audiumEngine->getLinkAudioDevice()) {
        auto& dspLoad = linkAudioDevice->getDspLoadMeter();
        loadMeter->setLoad(dspLoad.getLoad(), dspLoad.takePeak(audium::DspLoadMeter::headerMeter));
    }

    playButton->setToggleState(audiumEngine->getPlayListScheduler()->isPlaying(),
                               dontSendNotification);
    
    recordButton->setToggleState(audiumEngine->getPlayListScheduler()->isRecordingArmed(),
                                 dontSendNotification);
    
    if (audiumEngine->getPlayListScheduler()->isRecording()) {
        audiumEngine->getRecordingActionHandler()->onPlayListItemUpdate();
        audiumEngine->getPlayListScheduler()->getTempoProvider()->sendActionMessage(audium::updateArrangementAction);
    }
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
