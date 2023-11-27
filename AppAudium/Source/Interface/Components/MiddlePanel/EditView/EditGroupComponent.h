/*
  ==============================================================================

    EditGroupComponent.h
    Created: 27 Nov 2023 12:11:36pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MiddlePanel/GroupBaseComponent.h"
#include "Interface/ColourIds.h"


//==============================================================================
/*
*/
class EditGroupComponent  : public GroupBaseComponent
{
public:
        
    EditGroupComponent (std::shared_ptr<AudioGroup> group,
                         std::shared_ptr<AudiumEngine> audiumEngine,
                         std::shared_ptr<ZoomHandler> zoomHandler) :
        GroupBaseComponent(group, audiumEngine, zoomHandler)
    {
        refreshComponent(group);
    }    
    
    void refreshComponent (std::shared_ptr<AudioGroup> audioGroup, bool forceRebuildComponents = false) override
    {
        
    }

    void resized() override
    {

    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditGroupComponent)
};
