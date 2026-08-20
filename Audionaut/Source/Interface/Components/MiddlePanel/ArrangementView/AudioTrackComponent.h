//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include <memory>

#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Controls/RegionSelector.h"
#include "Interface/Components/MiddlePanel/AudioTrackBaseComponent.h"
#include "Interface/Models/ArrangementModel.h"
#include "Interface/Views/ClipFadeOverlay.h"

using namespace juce;

class AudioTrack;
class PlayListContainer;
class PlayListItemComponent;






//==============================================================================
/*
 
 Display an audio track as part of AudioTrackListBoxModel (playlist items on timeline).
 
 */
class AudioTrackComponent : public AudioTrackBaseComponent, public juce::DragAndDropTarget
{
public:
        
    AudioTrackComponent (std::shared_ptr<audium::AudioTrack> track,
                               std::shared_ptr<audium::AudiumEngine> audiumEngine,
                               std::shared_ptr<ZoomHandler> zoomHandler,
                               std::shared_ptr<RegionSelector> regionSelector) :
        AudioTrackBaseComponent(track, audiumEngine, zoomHandler, regionSelector)
    {
        model.reset(new ArrangementModel(audiumEngine, track, regionSelector, zoomHandler));

        // lane-wide layer above all clips: outside-the-clip fade ramps and
        // the lane-parented fade handles
        fadeOverlay = std::make_unique<ClipFadeOverlay>(track, audiumEngine, zoomHandler);
        addAndMakeVisible(fadeOverlay.get());

        refreshComponent(track);
    }
    
    void refreshComponent (std::shared_ptr<audium::AudioTrack> audioTrack, bool forceRebuildComponents = false) override;
    
    void updateContents(bool forceRebuildComponents = false);
    
    void resized() override;
    
    
    /// Drag n Drop:
    bool isInterestedInDragSource (const SourceDetails &dragSourceDetails) override;
    void itemDragEnter (const SourceDetails &dragSourceDetails) override;
    void itemDragMove (const SourceDetails &dragSourceDetails) override;
    void itemDragExit (const SourceDetails &dragSourceDetails) override;
    void itemDropped (const SourceDetails &dragSourceDetails) override;
    bool shouldDrawDragImageWhenOver () override { return true; }
    
    ArrangementModel* getModel() const { return model.get(); }

    ClipFadeOverlay* getFadeOverlay() const { return fadeOverlay.get(); }

private:
    class ItemComponent;


    std::unique_ptr<ArrangementModel> model;

    std::vector<std::unique_ptr<juce::Component>> itemComponents;

    std::unique_ptr<ClipFadeOverlay> fadeOverlay;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrackComponent)
    
};


