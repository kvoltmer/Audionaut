//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"

#include "Interface/Controls/LevelMeter.h"

/**
 * One channel's strip in the arrangement's channel view, a row of its track's
 * channel list: the console keys across the top, the level meter, and down
 * the right edge the amplitude scale of the waveform in the clip row beside
 * it. Gain, pan and routing are set in the mixer, the waveform size in the
 * track header's menu. A micro row shows the meter and the scale only.
 */
class ChannelComponent  :   public juce::Component,
                            private juce::Timer,
                            public juce::DragAndDropTarget
{
public:
    ChannelComponent (std::shared_ptr<audium::AudioTrack> audioTrack,
                      std::shared_ptr<audium::AudiumEngine> engine,
                      int rowNumber);
    ~ChannelComponent() override;

    /// The strip's measurements, in a row of AudiumLookAndFeel::channelsWidth.
    struct Layout
    {
        static constexpr int padding = 7;         ///< the left edge to the controls, and between the keys
        static constexpr int keysY = 9;           ///< the top row: record, monitor, solo, mute
        static constexpr int keySize = 15;
        static constexpr int meterInset = 3;      ///< the meter's top and bottom inside the row
        static constexpr int scaleWidth = 7;      ///< the waveform scale down the right edge, the row's full height
        static constexpr int meterWidth = 7;      ///< the meter's bar, left of the scale
        static constexpr int meterGap = 3;        ///< the bar to the scale
    };

    void refreshComponent(std::shared_ptr<audium::AudioTrack> audioTrack, int rowNumber, bool isRowSelected);
    void timerCallback() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    
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

    std::shared_ptr<audium::AudiumEngine> getEngine() const { return engine; }
    std::shared_ptr<audium::AudioTrack> getAudioTrack() const { return audioTrack; }
    
    static void configurePanSlider(juce::Slider *slider);

    /// The record toggle's rules, shared with the mixer strip: an input must be there, the
    /// arming is one undo step, and a running armed transport starts or stops the take.
    static bool audioInputAvailable (const std::shared_ptr<audium::AudiumEngine>& engine,
                                     const std::shared_ptr<audium::AudioTrack>& track, int channelNumber);
    static void setRecordEnabled (const std::shared_ptr<audium::AudiumEngine>& engine,
                                  const std::shared_ptr<audium::AudioTrack>& track, int channelNumber, bool enabled);

    /// The input or output routing menu of a channel, shared with the mixer strip: the default
    /// route, the current device's channels by name, and a saved route the device lacks (disabled).
    static juce::PopupMenu routingMenu (const std::shared_ptr<audium::AudiumEngine>& engine,
                                        const std::shared_ptr<audium::AudioTrack>& track, int channelNumber, bool input);

    /// One undo step: -1 restores the default route.
    static void setRoutingChannel (const std::shared_ptr<audium::AudioTrack>& track, int channelNumber, bool isInput, int newChannel);
    
private:
    /// The waveform's amplitude scale: ticks at the quarters of full scale, the centre line the
    /// longest, flush with the clip row the waveform is in. Full scale is where a full-scale
    /// sample reaches in that row (AudiumLookAndFeel::waveformScale).
    void drawWaveformScale (juce::Graphics& g, juce::Rectangle<int> area) const;

    std::shared_ptr<audium::AudioTrack> audioTrack;
    std::shared_ptr<audium::AudiumEngine> engine;

    juce::DrawableButton recordButton { "Record", juce::DrawableButton::ImageOnButtonBackground };
    juce::TextButton monitorButton { "I" }, soloButton { "S" }, muteButton { "M" };
    LevelMeter levelMeter { true, false };
    
    int rowNumber = 0;
    
    // used for timer updates
    int channelNumber = -1;
    
    bool insertAfter = false;
    bool insertBefore = false;

    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelComponent)
};
