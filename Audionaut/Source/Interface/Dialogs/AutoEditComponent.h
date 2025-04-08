//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.


#pragma once

#include <JuceHeader.h>

class AutoEditComponent  : public juce::Component,
                           public juce::ComboBox::Listener
{
public:
    AutoEditComponent ();
    ~AutoEditComponent() override;


    juce::Value& getDuration() const { return duration->getTextValue(); }
    juce::Value& getNumSegments() const { return numSegments->getTextValue(); }
    juce::Value& getMinSegmentLength() const { return segmentMin->getTextValue(); }
    juce::Value& getMaxSegmentLength() const { return segmentMax->getTextValue(); }
    juce::Value& getEditMode() const { return mode->getSelectedIdAsValue(); }

    void paint (juce::Graphics& g) override;
    void resized() override;
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;



private:


    //==============================================================================
    std::unique_ptr<juce::Label> juce__label;
    std::unique_ptr<juce::Label> juce__label2;
    std::unique_ptr<juce::Label> juce__label3;
    std::unique_ptr<juce::Label> juce__label4;
    std::unique_ptr<juce::TextEditor> duration;
    std::unique_ptr<juce::TextEditor> numSegments;
    std::unique_ptr<juce::TextEditor> segmentMin;
    std::unique_ptr<juce::TextEditor> segmentMax;
    std::unique_ptr<juce::ComboBox> mode;
    std::unique_ptr<juce::Label> juce__label5;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoEditComponent)
};
