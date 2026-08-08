//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Models/PlayListItemListBoxModel.h"

#include "Engine/AudiumEngine.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Group/AudioTrack.h"

class ZoomHandler;
class RegionSelector;
class DraggerControl;
class FadeInOutControl;
class AutoEditPreviewView;
class AutoEditOverlayControl;

//==============================================================================
/*
Display a PlayListItem within a AudioTrack
*/
class PlayListItemComponent  : public juce::Component, public juce::ChangeListener
{
public:
    PlayListItemComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine,
                          std::shared_ptr<audium::AudioTrack> audioTrack,
                          std::shared_ptr<audium::PlayListContainer> playListContainer,
                          std::shared_ptr<ZoomHandler> zoomHandler,
                          std::shared_ptr<RegionSelector> regionSelector);
    ~PlayListItemComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void changeListenerCallback (ChangeBroadcaster* source) override;
    
    std::shared_ptr<audium::PlayListItem> getPlayListItem() const { return playListItem; }
    void setPlayListItem(std::shared_ptr<audium::PlayListItem> item);
    
    DraggerControl* getDraggerControl() const;
    
    void updateUI(std::shared_ptr<audium::PlayListItem> playListItem);

    /**
     * @brief Refreshes the analysis (segmentation/BPM) overlay on every
     *        currently visible channel row, without rebuilding waveforms.
     */
    void refreshAnalysisDisplay();

    void mouseEnter (const MouseEvent& e) override;
    void mouseExit (const MouseEvent& e) override;
    
    int getNumAudioTrackChannels() const
    {
        return audioTrack->getNumAudioTrackChannels();
    }
    
private:
    // Feeds the merge preview of a pending auto edit to the overlay
    // (AnalysisProvider::getMergePreview for this clip's audio file).
    void refreshAutoEditPreview();

    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<audium::AudioTrack>     audioTrack;
    std::shared_ptr<audium::PlayListItem>   playListItem;
    std::shared_ptr<RegionSelector> regionSelector;

    std::unique_ptr<audium::ListBox> playListItemListBox;
    std::unique_ptr<PlayListItemListBoxModel> playListItemListBoxModel;


    std::unique_ptr<FadeInOutControl> fadeInControl;
    std::unique_ptr<FadeInOutControl> fadeOutControl;

    // Container stacked over the whole clip (header and all channel rows)
    // hosting overlays that must draw above everything else. Transparent to
    // the mouse itself, but its children may take clicks - the overlay
    // control does, the preview lines do not.
    std::unique_ptr<juce::Component> overlayContainer;
    std::unique_ptr<AutoEditPreviewView> autoEditPreviewView;

    // The pending auto edit's control (measures + Apply), shown while this
    // clip's merge preview is active.
    std::unique_ptr<AutoEditOverlayControl> autoEditOverlayControl;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListItemComponent)
};
