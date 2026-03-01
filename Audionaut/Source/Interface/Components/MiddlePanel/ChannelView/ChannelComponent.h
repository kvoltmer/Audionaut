//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/Resource/AudioResource.h"
#include "Engine/AudiumEngine.h"

#include "Interface/Controls/LevelMeter.h"

class ChannelComponent  :   public juce::Component,
                            private juce::Timer,
                            public juce::ComboBox::Listener,
                            public juce::DragAndDropTarget
{
public:
    ChannelComponent (std::shared_ptr<audium::AudioTrack> audioTrack,
                      std::shared_ptr<audium::AudiumEngine> engine,
                      int rowNumber);
    ~ChannelComponent() override;


    void refreshComponent(std::shared_ptr<audium::AudioTrack> audioTrack, int rowNumber, bool isRowSelected);
    void timerCallback() override;
    void stopTheTimer() { stopTimer(); }
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& key) override;
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (juce::DragAndDropContainer* container = juce::DragAndDropContainer::findParentDragContainerFor(this)) {
            container->startDragging("ChannelComponent", this);
        }
    }
        
    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) override;
    
    void updateInsertLines(const juce::DragAndDropTarget::SourceDetails &dragSourceDetails);
    
    void hideInsertLines()
    {
        insertBefore = false;
        insertAfter = false;
        
        repaint();
    }
    void itemDragEnter (const SourceDetails &dragSourceDetails) override
    {
        updateInsertLines(dragSourceDetails);
    }
    
    void itemDragMove (const SourceDetails &dragSourceDetails) override
    {
        updateInsertLines(dragSourceDetails);
    }
    
    void itemDragExit (const SourceDetails &dragSourceDetails) override
    {
        hideInsertLines();
    }
    
    void itemDropped (const SourceDetails &dragSourceDetails) override;
    
    bool shouldDrawDragImageWhenOver () override
    {
        return true;
    }
    
    bool isChannelSelected() const;
    

    // Binary resources:
    static const char* channelScale_png;
    static const int channelScale_pngSize;

    std::shared_ptr<audium::AudiumEngine> getEngine() const { return engine; }
    std::shared_ptr<audium::AudioTrack> getAudioTrack() const { return audioTrack; }
    
    static void configureVolumeSlider(juce::Slider *slider, double dbMax = 6.0);
    static void configurePanSlider(juce::Slider *slider);
    
    bool audioInputAvailable(int channelNumber);
    void setRecordEnabled(int channelNumber, bool bEnabled);
    
private:
    std::shared_ptr<audium::AudioTrack> audioTrack;
    std::shared_ptr<audium::AudiumEngine> engine;
    std::unique_ptr<LevelMeter> levelMeter;
    std::unique_ptr<juce::ComboBox> channelSizeComboBox;
    std::unique_ptr<juce::Slider> volumeSlider;
    std::unique_ptr<juce::Slider> panSlider;
    std::unique_ptr<juce::ImageButton> volumeScaleButton;
    std::unique_ptr<juce::TextButton> muteButton;
    std::unique_ptr<juce::TextButton> soloButton;
    std::unique_ptr<juce::DrawableButton> recordButton;
    std::unique_ptr<juce::TextButton> monitorButton;
    
    int rowNumber = 0;
    
    // used for timer updates
    int channelNumber = -1;
    
    bool insertAfter = false;
    bool insertBefore = false;

    
    // linear scaling
    static const double scale_linear(const double dVal, const double dMin, const double dMax)
    {
        return dMin + (dVal * abs(dMax - dMin));
    }

    // reverse linear scaling
    static double reverse_linear(const double dVal, const double dMin, const double dMax)
    {
        return abs(dVal - dMin) / abs(dMax - dMin);
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelComponent)
};

