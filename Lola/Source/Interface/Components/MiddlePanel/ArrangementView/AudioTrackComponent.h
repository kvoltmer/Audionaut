//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
        
    AudioTrackComponent (std::shared_ptr<audium::AudioTrack> track,
                               std::shared_ptr<audium::AudiumEngine> audiumEngine,
                               std::shared_ptr<ZoomHandler> zoomHandler,
                               std::shared_ptr<RegionSelector> regionSelector) :
        AudioTrackBaseComponent(track, audiumEngine, zoomHandler, regionSelector)
    {
        model.reset(new ArrangementModel(audiumEngine, track, regionSelector, zoomHandler));
        refreshComponent(track);
    }
    
    void refreshComponent (std::shared_ptr<audium::AudioTrack> audioTrack, bool forceRebuildComponents = false) override;
    
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


