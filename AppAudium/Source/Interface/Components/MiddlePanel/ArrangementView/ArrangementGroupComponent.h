/*
  ==============================================================================

    ArrangementGroupComponent.h
    Created: 27 Nov 2022 3:25:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>

#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Controls/RegionSelector.h"
#include "Interface/Components/MiddlePanel/GroupBaseComponent.h"

using namespace juce;

class AudioGroup;
class PlayListContainer;
class PlayListItemComponent;

//==============================================================================
/*
 
 Display Playlist items on Timeline
 
 Display a AudioGroup as part of GroupListBoxModel.
 
 The ArrangementGroupComponent contains multiple playlist items (regions) in the Timeline.
 */
class ArrangementGroupComponent : public GroupBaseComponent, public juce::DragAndDropTarget
{
public:
        
    ArrangementGroupComponent (std::shared_ptr<AudioGroup> group,
                               std::shared_ptr<AudiumEngine> audiumEngine,
                               std::shared_ptr<ZoomHandler> zoomHandler,
                               std::shared_ptr<RegionSelector> regionSelector) :
        GroupBaseComponent(group, audiumEngine, zoomHandler, regionSelector)
    {
        refreshComponent(group);
    }
    
    void refreshComponent (std::shared_ptr<AudioGroup> audioGroup, bool forceRebuildComponents = false) override;
    
    void resized() override;
    
    
    /// Drag n Drop:
    ///----------------------------------
    bool isInterestedInDragSource (const SourceDetails &dragSourceDetails) override;
    
    void itemDragEnter (const SourceDetails &dragSourceDetails) override
    {
    }
    
    void itemDragMove (const SourceDetails &dragSourceDetails) override;
    void itemDragExit (const SourceDetails &dragSourceDetails) override;
    void itemDropped (const SourceDetails &dragSourceDetails) override;
    
    bool shouldDrawDragImageWhenOver () override
    {
        return true;
    }

    void filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY) override;

private:
    
    bool mustRebuildComponents() const;
    void rebuildComponents();
    
    std::vector<std::shared_ptr<PlayListItemComponent>> playListItemComponents;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementGroupComponent)
    
};
