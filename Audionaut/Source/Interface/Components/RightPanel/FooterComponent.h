//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.


#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/Playback/Playback.h"

class FooterComponent  : public juce::Component, private juce::Timer
{
public:
    FooterComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine_) :
        audiumEngine(audiumEngine_)
    {
        totalLengthLabel.reset(new juce::Label());
        addAndMakeVisible(totalLengthLabel.get());
        totalLengthLabel->setFont (juce::FontOptions { 13.0f });
        totalLengthLabel->setJustificationType (juce::Justification::centredLeft);
        totalLengthLabel->setEditable (false, false, false);
        totalLengthLabel->setColour (juce::Label::backgroundColourId, findColour(audium::backgroundColourId));
        totalLengthLabel->setColour (juce::TextEditor::textColourId, juce::Colours::black);
        
        numVoicesLabel.reset(new juce::Label());
        addAndMakeVisible(numVoicesLabel.get());
        numVoicesLabel->setFont (juce::FontOptions { 13.0f });
        numVoicesLabel->setJustificationType (juce::Justification::centredLeft);
        numVoicesLabel->setEditable (false, false, false);
        numVoicesLabel->setColour (juce::Label::backgroundColourId, findColour(audium::backgroundColourId));
        numVoicesLabel->setColour (juce::TextEditor::textColourId, juce::Colours::black);
        
        startTimer(50);
    }

    ~FooterComponent() override
    {
        stopTimer();
    }

    void paint (juce::Graphics& g) override
    {
    }

    void resized() override
    {
        auto width = getWidth() / 2;
        
        juce::Rectangle<int> totalLengthBounds(0, 0, width, getHeight());
        totalLengthLabel->setBounds(totalLengthBounds);
        
        juce::Rectangle<int> numVoiceBounds(width, 0, width, getHeight());
        numVoicesLabel->setBounds(numVoiceBounds);

    }
    
    void timerCallback() override
    {
        auto timeSec = audiumEngine->getPlayListScheduler()->getTotalLength(audium::seconds);
        totalLengthLabel->setText(audium::TempoProvider::secondsToFormattedString(timeSec), juce::dontSendNotification);
        
        auto numVoices = audiumEngine->getPlayListScheduler()->getPlayback()->getNumVoices();
        numVoicesLabel->setText("Voices " + juce::String(numVoices), juce::dontSendNotification);
    }

private:
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::unique_ptr<juce::Label> totalLengthLabel;
    std::unique_ptr<juce::Label> numVoicesLabel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FooterComponent)
};
