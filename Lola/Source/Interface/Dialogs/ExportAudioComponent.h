/*
  ==============================================================================

    ExportAudioComponent.h
    Created: 28 Oct 2024 12:02:03pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

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
    ExportAudioComponent(std::shared_ptr<AudiumEngine> engine) :
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
                
        Rectangle<int> r (proportionOfWidth (0.5f), 20, proportionOfWidth (0.6f), 3000);
        
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
    }
    
    void labelTextChanged (Label* labelThatHasChanged) override
    {
        
    }
    
    void update()
    {
        if (auto* currentDevice = audiumEngine->getAudioDeviceManager()->getCurrentAudioDevice())
        {
            updateSampleRateComboBox(currentDevice);
        }
        updateOutputChanComboBox();
        
        resized();
    }
    
    void updateSampleRateComboBox (AudioIODevice* currentDevice)
    {
        if (sampleRateDropDown == nullptr)
        {
            sampleRateDropDown = std::make_unique<ComboBox>();
            addAndMakeVisible (sampleRateDropDown.get());

            sampleRateLabel = std::make_unique<juce::Label> (String{}, TRANS ("Sample rate:"));
            sampleRateLabel->setFont (juce::FontOptions (AudiumLookAndFeel::labelFontSize));
            sampleRateLabel->attachToComponent (sampleRateDropDown.get(), true);
        }
        else
        {
            sampleRateDropDown->clear();
            sampleRateDropDown->onChange = nullptr;
        }
        
        const auto getFrequencyString = [] (int rate) { return String (rate) + " Hz"; };

        for (auto rate : availableSampleRates)
        {
            const auto intRate = roundToInt (rate);
            sampleRateDropDown->addItem (getFrequencyString (intRate), intRate);
        }

        // default is the sample rate of the device
        const auto intRate = roundToInt (currentDevice->getCurrentSampleRate());
        sampleRateDropDown->setText (getFrequencyString (intRate), dontSendNotification);

        //sampleRateDropDown->onChange = [this] { updateConfig (false, false, true, false); };
    }
    
    void updateOutputChanComboBox ()
    {
        if (outputChanDropDown == nullptr)
        {
            outputChanDropDown = std::make_unique<ComboBox>();
            addAndMakeVisible (outputChanDropDown.get());

            outputChanLabel = std::make_unique<juce::Label> (String{}, TRANS ("Output channels:"));
            outputChanLabel->setFont (juce::FontOptions (AudiumLookAndFeel::labelFontSize));
            outputChanLabel->attachToComponent (outputChanDropDown.get(), true);
        }
        else
        {
            outputChanDropDown->clear();
            outputChanDropDown->onChange = nullptr;
        }
        
        int i = 1;
        for (auto chan : availableOutputChans)
        {
            //const auto intRate = roundToInt (rate);
            outputChanDropDown->addItem (chan, i++);
        }

        // default
        outputChanDropDown->setText (availableOutputChans[1], dontSendNotification);

    }

    
    juce::Value& getSampleRate() const { return sampleRateDropDown->getSelectedIdAsValue(); }
    juce::Value& getOutputChannels() const { return outputChanDropDown->getSelectedIdAsValue(); }
    
private:
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    
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
        "multi-channel"
    };
    

    
    std::unique_ptr<juce::Label> sampleRateLabel, outputChanLabel;
    std::unique_ptr<ComboBox> sampleRateDropDown, outputChanDropDown;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExportAudioComponent)
};
