//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.


#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/Playback/Playback.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Analysis/AnalysisWorker.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

class FooterComponent  : public juce::Component, private juce::Timer
{
public:
    FooterComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine_) :
        audiumEngine(audiumEngine_)
    {
        totalLengthLabel.reset(new juce::Label());
        addAndMakeVisible(totalLengthLabel.get());
        totalLengthLabel->setFont (AudiumLookAndFeel::withTabularFigures (juce::FontOptions { 13.0f }));
        totalLengthLabel->setJustificationType (juce::Justification::centredLeft);
        totalLengthLabel->setEditable (false, false, false);
        
        numVoicesLabel.reset(new juce::Label());
        addAndMakeVisible(numVoicesLabel.get());
        numVoicesLabel->setFont (AudiumLookAndFeel::withTabularFigures (juce::FontOptions { 13.0f }));
        numVoicesLabel->setJustificationType (juce::Justification::centredLeft);
        numVoicesLabel->setEditable (false, false, false);

        // Background-analysis status. Added last so it draws on top of the two
        // labels above, and shown only while the AnalysisWorker is busy.
        analysisStatusLabel.reset(new juce::Label());
        addChildComponent(analysisStatusLabel.get());
        // Also tabular: the trailing "(N remaining)" counts down while this is visible.
        analysisStatusLabel->setFont (AudiumLookAndFeel::withTabularFigures (juce::FontOptions { 13.0f }));
        analysisStatusLabel->setJustificationType (juce::Justification::centredLeft);
        analysisStatusLabel->setEditable (false, false, false);


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

        // Overlays the full footer, on top of both labels above.
        analysisStatusLabel->setBounds(0, 0, getWidth(), getHeight());
    }

    void timerCallback() override
    {
        auto timeSec = audiumEngine->getPlayListScheduler()->getTotalLength(audium::seconds);
        totalLengthLabel->setText(audium::TempoProvider::secondsToFormattedString(timeSec), juce::dontSendNotification);

        auto numVoices = audiumEngine->getPlayListScheduler()->getPlayback()->getNumVoices();
        numVoicesLabel->setText("Voices " + juce::String(numVoices), juce::dontSendNotification);

        auto analysisWorker = audiumEngine->getAudioResourceContainer()->getAnalysisWorker();
        const bool busy = analysisWorker != nullptr && analysisWorker->isBusy();
        if (busy) {
            // Small ASCII "sweep" animation, one step every 4 timer ticks
            // (startTimer(50) above -> ~200ms per step) so it reads as
            // moving rather than flickering.
            static constexpr const char* sweepFrames[] = { "[=   ]", "[ =  ]", "[  = ]", "[   =]",
                                                            "[  = ]", "[ =  ]" };
            constexpr auto numFrames = sizeof(sweepFrames) / sizeof(sweepFrames[0]);
            const auto frame = sweepFrames[(analysisAnimationTick++ / 4) % numFrames];

            auto analysisType = analysisWorker->getCurrentAnalysisType();
            auto typeLabel = analysisType.has_value() ? audium::analysisTypeToString(*analysisType) : "";

            std::string txt = std::string(frame) + " Analysing " + typeLabel + ": "
                             + analysisWorker->getCurrentFileName().toStdString() + " ";
            txt += "(" + std::to_string(analysisWorker->getRemainingCount()) + " remaining)";
            analysisStatusLabel->setText(txt, juce::dontSendNotification);
        }
        else {
            analysisAnimationTick = 0;
        }
        analysisStatusLabel->setVisible(busy);
    }

private:
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::unique_ptr<juce::Label> totalLengthLabel;
    std::unique_ptr<juce::Label> numVoicesLabel;
    std::unique_ptr<juce::Label> analysisStatusLabel;
    int analysisAnimationTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FooterComponent)
};
