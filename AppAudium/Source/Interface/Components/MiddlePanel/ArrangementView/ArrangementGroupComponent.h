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
 
 Display Playlist items in Timeline
 
 Display a AudioGroup as part of AudioGroupListBoxModel.
 
 A AudioGroup may contain multiple regions in the Timeline. The PlayListContainer holds the region information
 */
class ArrangementGroupComponent : public GroupBaseComponent
{
public:
        
    ArrangementGroupComponent (std::shared_ptr<AudioGroup> audioGroup,
                         std::shared_ptr<AudiumEngine> audiumEngine,
                         std::shared_ptr<ZoomHandler> zoomHandler);
    
    void refreshComponent (std::shared_ptr<AudioGroup> audioGroup, bool forceRebuildComponents = false) override;
    
    void resized() override;
    
private:
    
    bool mustRebuildComponents() const;
    void rebuildComponents();
    
    std::vector<std::shared_ptr<PlayListItemComponent>> playListItemComponents;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementGroupComponent)
    
};
