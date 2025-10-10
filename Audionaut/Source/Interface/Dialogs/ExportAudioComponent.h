//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Interface/Controls/DefaultLabel.h"
#include "Engine/AudiumEngine.h"


//==============================================================================
/*
*/
class ExportAudioComponent  : public juce::Component, public juce::Label::Listener
{
public:
    ExportAudioComponent(std::shared_ptr<audium::AudiumEngine> engine) :
        audiumEngine(engine)
    {
        setSize(300, 100);
    }

    ~ExportAudioComponent() override
    {
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));   // clear the background
    }

    void resized() override
    {
                
        juce::Rectangle<int> r(proportionOfWidth(0.5f), 20, proportionOfWidth(0.4f), 3000);
        
        const int h = 23;
        const int space = h / 4;
        
        if (sampleRateDropDown != nullptr) {
            sampleRateDropDown->setBounds (r.removeFromTop (h));
            r.removeFromTop (space);
        }
        
        if (outputChanDropDown != nullptr) {
            outputChanDropDown->setBounds (r.removeFromTop (h));
            r.removeFromTop (space);
        }
        
        if (bitDepthDropDown != nullptr) {
            bitDepthDropDown->setBounds (r.removeFromTop (h));
            r.removeFromTop (space);
        }
    }
    
    void labelTextChanged (Label* labelThatHasChanged) override
    {
        
    }
    
    void update()
    {
        if (auto* currentDevice = audiumEngine->getAudioDeviceManager()->getCurrentAudioDevice()) {
            updateSampleRateComboBox(currentDevice);
        }
        updateOutputChanComboBox();
        updateBitDepthComboBox();
        
        resized();
    }
    
    void updateSampleRateComboBox (AudioIODevice* currentDevice)
    {
        if (sampleRateDropDown == nullptr) {
            sampleRateDropDown = std::make_unique<ComboBox>();
            addAndMakeVisible (sampleRateDropDown.get());

            sampleRateLabel = std::make_unique<juce::Label> (String{}, TRANS ("Sample rate:"));
            sampleRateLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
            sampleRateLabel->attachToComponent (sampleRateDropDown.get(), true);
        }
        else {
            sampleRateDropDown->clear();
        }
        
        const auto getFrequencyString = [] (int rate) { return String (rate) + " Hz"; };

        for (auto rate : availableSampleRates) {
            const auto intRate = roundToInt (rate);
            sampleRateDropDown->addItem (getFrequencyString (intRate), intRate);
        }

        // default is the sample rate of the device
        const auto intRate = roundToInt (currentDevice->getCurrentSampleRate());
        sampleRateDropDown->setText (getFrequencyString (intRate), dontSendNotification);
    }
    
    void updateOutputChanComboBox ()
    {
        if (outputChanDropDown == nullptr) {
            outputChanDropDown = std::make_unique<ComboBox>();
            addAndMakeVisible (outputChanDropDown.get());

            outputChanLabel = std::make_unique<juce::Label> (String{}, TRANS ("Output channels:"));
            outputChanLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
            outputChanLabel->attachToComponent (outputChanDropDown.get(), true);
        }
        else {
            outputChanDropDown->clear();
        }
        
        int totalChannels = audiumEngine->getAudioTrackContainer()->getNumAudioTrackChannels();
        
        int i = 1;
        for (auto chan : availableOutputChans) {
            if (chan == "multi-mono") {
                chan += " (" + std::to_string(totalChannels) + " files)";
            }
                
            outputChanDropDown->addItem (chan, i++);
        }

        // default is stereo
        outputChanDropDown->setText (availableOutputChans[1], dontSendNotification);

    }
    
    void updateBitDepthComboBox ()
    {
        if (bitDepthDropDown == nullptr) {
            bitDepthDropDown = std::make_unique<ComboBox>();
            addAndMakeVisible (bitDepthDropDown.get());

            bitDepthLabel = std::make_unique<juce::Label> (String{}, TRANS ("Bit depth:"));
            bitDepthLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
            bitDepthLabel->attachToComponent (bitDepthDropDown.get(), true);
        }
        else {
            bitDepthDropDown->clear();
        }
        
        const auto getBitDepthString = [] (int depth) { return String (depth) + " Bits"; };

        for (auto bits : availableBitDepths) {
            bitDepthDropDown->addItem (getBitDepthString (bits), bits);
        }

        // default is 24 bits
        bitDepthDropDown->setText (getBitDepthString (24), dontSendNotification);
    }

    
    juce::Value& getSampleRate() const { return sampleRateDropDown->getSelectedIdAsValue(); }
    juce::Value& getOutputChannels() const { return outputChanDropDown->getSelectedIdAsValue(); }
    juce::Value& getBitDepth() const { return bitDepthDropDown->getSelectedIdAsValue(); }
    
private:
    
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    
    const std::vector<double> availableSampleRates = {
        22050.0,
        32000.0,
        44100.0,
        48000.0,
        88200.0,
        96000.0,
        176400.0,
        192000.0,
        352800.0,
        384000.0
    };
    
    const std::vector<std::string> availableOutputChans = {
        "mono",
        "stereo",
        "multi-channel",
        "multi-mono"
    };
    
    const std::vector<unsigned int> availableBitDepths = {
        16,
        24,
        32
    };

    
    std::unique_ptr<juce::Label> sampleRateLabel, outputChanLabel, bitDepthLabel;
    std::unique_ptr<ComboBox> sampleRateDropDown, outputChanDropDown, bitDepthDropDown;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExportAudioComponent)
};
