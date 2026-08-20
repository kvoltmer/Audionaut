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
class DraggingHandle;
class AutoEditPreviewView;
class AutoEditOverlayControl;

//==============================================================================
/*
Display a PlayListItem within a AudioTrack
*/
class PlayListItemComponent  : public juce::Component,
                               public juce::ChangeListener,
                               public juce::ComponentListener
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

    /**
     * @brief Repaints every currently visible channel row of the clip
     *        (e.g. so the fade overlay follows a fade-handle drag).
     */
    void repaintClipRows();

    void mouseEnter (const MouseEvent& e) override;
    void mouseExit (const MouseEvent& e) override;

    // Follows the parent ItemComponent (the clip rect) so the lane-parented
    // fade handles track zoom/scroll relayouts and live dragger drags.
    void componentMovedOrResized (juce::Component& component, bool wasMoved, bool wasResized) override;
    void componentVisibilityChanged (juce::Component& component) override;
    
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


    std::unique_ptr<DraggingHandle> fadeInControl;
    std::unique_ptr<DraggingHandle> fadeOutControl;
    std::unique_ptr<DraggingHandle> fadeInStartControl;
    std::unique_ptr<DraggingHandle> fadeOutEndControl;

    // Re-positions every fade handle from the item's dynamics; a fade
    // setter push can cascade across all four values.
    void syncFadeControls();

    // Parents the two bottom handles into the lane's ClipFadeOverlay (they
    // may sit outside the clip rect) and registers for the clip rect's
    // moves; safe to call repeatedly.
    void attachHandlesToLane();

    // Repaints the lane overlay that draws the outside-the-clip ramp parts.
    void repaintFadeOverlay();

    // The parent ItemComponent currently observed via ComponentListener.
    juce::Component* observedClipRect = nullptr;

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
