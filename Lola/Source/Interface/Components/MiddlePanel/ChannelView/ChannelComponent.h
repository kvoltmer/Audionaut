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
    ChannelComponent (std::shared_ptr<AudioTrack> audioTrack,
                      std::shared_ptr<AudiumEngine> engine,
                      int rowNumber);
    ~ChannelComponent() override;


    void refreshComponent(std::shared_ptr<AudioTrack> audioTrack, int rowNumber, bool isRowSelected);
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
    

    // Binary resources:
    static const char* channelScale_png;
    static const int channelScale_pngSize;

    enum { moveChannelToNewTrackId = 0xf836743, reservedId = 0xf836744 };

    std::shared_ptr<AudiumEngine> getEngine() const { return engine; }
    std::shared_ptr<AudioTrack> getAudioTrack() const { return audioTrack; }
    
    static void configureVolumeSlider(juce::Slider *slider, double dbMax = 6.0);
    static void configurePanSlider(juce::Slider *slider);
    
private:
    std::shared_ptr<AudioTrack> audioTrack;
    std::shared_ptr<AudiumEngine> engine;
    std::unique_ptr<LevelMeter> levelMeter;
    std::unique_ptr<juce::ComboBox> channelSizeComboBox;
    std::unique_ptr<juce::Slider> volumeSlider;
    std::unique_ptr<juce::Slider> panSlider;
    std::unique_ptr<juce::ImageButton> volumeScaleButton;
    std::unique_ptr<juce::TextButton> muteButton;
    std::unique_ptr<juce::TextButton> soloButton;
    
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

