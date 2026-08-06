//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.


#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/AutoEdit/AutoEdit.h"

class AutoEditComponent  : public juce::Component,
                           public juce::ChangeListener
{
public:
    AutoEditComponent (std::shared_ptr<audium::AudiumEngine> audiumEngine);
    ~AutoEditComponent() override;


    audium::AutoEditConfig& getAutoEditConfig();

    void resized() override;

    // Refreshes the hosting window's title and the merge preview from the
    // current selection.
    void update();

    // The window title: "Auto Edit <track name> : <playlist item name>".
    juce::String getTitle() const;

    // Follows selection changes made elsewhere in the application.
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    // Invoked when the user presses the Apply button.
    std::function<void()> onApply;

private:

    const audium::PlayListItem* getSelectedPlaylistItem() const;

    // The selected playlist item, or the first sufficiently long item on the
    // default track when no playlist item is selected.
    const audium::PlayListItem* getTargetPlaylistItem() const;

    // Publishes the boundaries the current config would cut at, so the
    // arrangement previews them (see AutoEdit::previewAutoEdit).
    void updatePreview();

    std::shared_ptr<audium::AudiumEngine> audiumEngine;

    audium::AutoEditConfig config;

    std::unique_ptr<juce::Label> messageLabel;
    std::unique_ptr<juce::TextButton> applyButton;

    // Whether the edited clip is replaced in the arrangement by its segments.
    std::unique_ptr<juce::ToggleButton> replacePlayListItem;

    std::unique_ptr<juce::Slider> numSegments, segmentMin, segmentMax;
    std::unique_ptr<juce::Label> numSegmentsLabel, segmentMinLabel, segmentMaxLabel;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoEditComponent)
};
