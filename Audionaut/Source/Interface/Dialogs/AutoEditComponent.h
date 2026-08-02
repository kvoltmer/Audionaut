//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.


#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/AutoEdit/AutoEdit.h"

class AutoEditComponent  : public juce::Component, public juce::ComboBox::Listener
{
public:
    AutoEditComponent (std::shared_ptr<audium::AudiumEngine> audiumEngine);
    ~AutoEditComponent() override = default;


    audium::AutoEditConfig& getAutoEditConfig();
    
    void resized() override;

    void update();
    
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;
    
private:

    const audium::PlayListItem* getSelectedPlaylistItem() const;
    void updateSelectedTrack();
    void updateSelectedPlaylistItem();

    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    
    audium::AutoEditConfig config;
    
    std::unique_ptr<juce::ComboBox> selectedTrack;
    std::unique_ptr<juce::Label> selectedTrackLabel;
    
    std::unique_ptr<juce::ComboBox> selectedPlaylistItem;
    std::unique_ptr<juce::Label> selectedPlaylistItemLabel;

    // Which implementation picks the cut points. Native needs nothing beyond
    // the analyses already cached; Python shells out to gaborgandalf and is
    // kept so the two can be compared on the same audio.
    std::unique_ptr<juce::ComboBox> selectedSource;
    std::unique_ptr<juce::Label> selectedSourceLabel;

    
    std::unique_ptr<juce::Slider> numSegments, segmentMin, segmentMax;
    std::unique_ptr<juce::Label> numSegmentsLabel, segmentMinLabel, segmentMaxLabel;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoEditComponent)
};
