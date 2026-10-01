//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Interface/Controls/DefaultLabel.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Export/ExportFormat.h"


class ExportAudioComponent  : public juce::Component
{
public:
    ExportAudioComponent(std::shared_ptr<audium::AudiumEngine> engine) :
        audiumEngine(engine)
    {
        setSize(300, 130);
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
        
        if (formatDropDown != nullptr) {
            formatDropDown->setBounds (r.removeFromTop (h));
            r.removeFromTop (space);
        }
        
        if (sampleRateDropDown != nullptr) {
            sampleRateDropDown->setBounds (r.removeFromTop (h));
            r.removeFromTop (space);
        }
        
        if (outputChanDropDown != nullptr) {
            outputChanDropDown->setBounds (r.removeFromTop (h));
            r.removeFromTop (space);
        }
        
        // bit depth (lossless) and quality (lossy) share the last row; only
        // the one the format takes is visible
        auto lastRow = r.removeFromTop (h);
        if (bitDepthDropDown != nullptr)
            bitDepthDropDown->setBounds (lastRow);
        if (qualityDropDown != nullptr)
            qualityDropDown->setBounds (lastRow);
    }
    
    void update()
    {
        updateFormatComboBox();
        if (auto* currentDevice = audiumEngine->getAudioDeviceManager()->getCurrentAudioDevice()) {
            updateSampleRateComboBox(currentDevice);
        }
        updateOutputChanComboBox();
        updateFormatDependentRows();
        
        resized();
    }
    
    void updateFormatComboBox()
    {
        if (formatDropDown == nullptr) {
            formatDropDown = std::make_unique<ComboBox>();
            addAndMakeVisible (formatDropDown.get());

            formatLabel = std::make_unique<juce::Label> (String{}, TRANS ("Format:"));
            formatLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
            formatLabel->attachToComponent (formatDropDown.get(), true);

            for (auto format : audium::allExportFormats()) {
                auto name = audium::formatName (format);
                if (format == audium::ExportFormat::flac)
                    name << " (lossless)";
                if (audium::isLossy (format))
                    name << " (lossy)";
                formatDropDown->addItem (name, formatId (format));
            }

            // default is WAV; later openings keep the last choice
            formatDropDown->setSelectedId (formatId (audium::ExportFormat::wav), dontSendNotification);

            // bit depth or quality, and which depths, depend on the format
            formatDropDown->onChange = [this] { updateFormatDependentRows(); };
        }
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
    
    /** Shows the bit depths a lossless format takes, or the quality steps
        of a lossy one, in the last row. */
    void updateFormatDependentRows()
    {
        const auto lossy = audium::isLossy (getFormat());
        if (lossy)
            updateQualityComboBox();
        else
            updateBitDepthComboBox();

        // the attached labels follow their combo box's visibility
        if (bitDepthDropDown != nullptr)
            bitDepthDropDown->setVisible (! lossy);
        if (qualityDropDown != nullptr)
            qualityDropDown->setVisible (lossy);

        // the quality row is created on first use, after the last layout
        resized();
    }

    void updateQualityComboBox()
    {
        if (qualityDropDown == nullptr) {
            qualityDropDown = std::make_unique<ComboBox>();
            addChildComponent (qualityDropDown.get());

            qualityLabel = std::make_unique<juce::Label> (String{}, TRANS ("Quality:"));
            qualityLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
            qualityLabel->attachToComponent (qualityDropDown.get(), true);
        }

        // keep the current choice when the steps stay the same
        auto selected = qualityDropDown->getSelectedId();
        qualityDropDown->clear (dontSendNotification);

        // ids are the quality index + 1 (an id of 0 means nothing selected)
        const auto steps = audium::qualityOptions (getFormat());
        for (auto i = 0; i < steps.size(); i++)
            qualityDropDown->addItem (steps[i], i + 1);

        // not isPositiveAndNotGreaterThan: that counts 0 - nothing selected
        // yet - as valid, and the combo would stay empty
        if (selected < 1 || selected > steps.size())
            selected = audium::defaultQuality (getFormat()) + 1;
        qualityDropDown->setSelectedId (selected, dontSendNotification);
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
        
        // keep the current choice where the format allows it, else 24 bits
        auto selected = bitDepthDropDown->getSelectedId();
        bitDepthDropDown->clear (dontSendNotification);
        
        const auto getBitDepthString = [] (int depth) { return String (depth) + " Bits"; };
        const auto supported = audium::supportedBitDepths (getFormat());

        for (auto bits : availableBitDepths) {
            if (supported.contains ((int) bits))
                bitDepthDropDown->addItem (getBitDepthString ((int) bits), (int) bits);
        }

        if (! supported.contains (selected))
            selected = 24;
        bitDepthDropDown->setSelectedId (selected, dontSendNotification);
    }

    
    audium::ExportFormat getFormat() const
    {
        if (formatDropDown != nullptr)
            for (auto format : audium::allExportFormats())
                if (formatDropDown->getSelectedId() == formatId (format))
                    return format;
        return audium::ExportFormat::wav;
    }

    /** The quality index for a lossy format (see audium::qualityOptions). */
    int getQuality() const
    {
        if (qualityDropDown == nullptr || qualityDropDown->getSelectedId() == 0)
            return audium::defaultQuality (getFormat());
        return qualityDropDown->getSelectedId() - 1;
    }
    
    juce::Value& getSampleRate() const { return sampleRateDropDown->getSelectedIdAsValue(); }
    juce::Value& getOutputChannels() const { return outputChanDropDown->getSelectedIdAsValue(); }
    juce::Value& getBitDepth() const { return bitDepthDropDown->getSelectedIdAsValue(); }
    
private:
    
    static int formatId (audium::ExportFormat format) { return (int) format + 1; }
    
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

    
    std::unique_ptr<juce::Label> formatLabel, sampleRateLabel, outputChanLabel, bitDepthLabel, qualityLabel;
    std::unique_ptr<ComboBox> formatDropDown, sampleRateDropDown, outputChanDropDown, bitDepthDropDown, qualityDropDown;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExportAudioComponent)
};
