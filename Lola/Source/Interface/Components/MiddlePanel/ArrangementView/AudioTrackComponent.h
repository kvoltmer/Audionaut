/*
  ==============================================================================

    AudioTrackComponent.h
    Created: 27 Nov 2022 3:25:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>

#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Controls/RegionSelector.h"
#include "Interface/Components/MiddlePanel/AudioTrackBaseComponent.h"
#include "Interface/Models/ArrangementModel.h"

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
        
    AudioTrackComponent (std::shared_ptr<AudioTrack> track,
                               std::shared_ptr<AudiumEngine> audiumEngine,
                               std::shared_ptr<ZoomHandler> zoomHandler,
                               std::shared_ptr<RegionSelector> regionSelector) :
        AudioTrackBaseComponent(track, audiumEngine, zoomHandler, regionSelector)
    {
        model.reset(new ArrangementModel(audiumEngine, track, regionSelector, zoomHandler));
        refreshComponent(track);
    }
    
    void refreshComponent (std::shared_ptr<AudioTrack> audioTrack, bool forceRebuildComponents = false) override;
    
    void updateContents();
    
    void resized() override;
    
    
    /// Drag n Drop:
    bool isInterestedInDragSource (const SourceDetails &dragSourceDetails) override;
    void itemDragEnter (const SourceDetails &dragSourceDetails) override;
    void itemDragMove (const SourceDetails &dragSourceDetails) override;
    void itemDragExit (const SourceDetails &dragSourceDetails) override;
    void itemDropped (const SourceDetails &dragSourceDetails) override;
    bool shouldDrawDragImageWhenOver () override { return true; }
    
    ArrangementModel* getModel() const { return model.get(); }
    
private:
    class ItemComponent;
    
    
    std::unique_ptr<ArrangementModel> model;

    std::vector<std::unique_ptr<juce::Component>> itemComponents;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrackComponent)
    
};


