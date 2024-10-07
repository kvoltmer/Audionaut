/*
  ==============================================================================

    GroupBaseComponent.cpp
    Created: 27 Nov 2023 12:23:48pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "GroupBaseComponent.h"


#include "Util/EngineAccess.h"
#include "Interface/Controls/AudioGroupListBox.h"
#include "Interface/Components/MiddlePanel/ArrangementView/PlayListItemComponent.h"
#include "Interface/ColourIds.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Interface/Handlers/SnapToGridHandler.h"

using namespace audium;


GroupBaseComponent::GroupBaseComponent (std::shared_ptr<AudioGroup> group,
                                        std::shared_ptr<AudiumEngine> audiumEngine,
                                        std::shared_ptr<ZoomHandler> zoomHandler,
                                        std::shared_ptr<RegionSelector> regionSelector) :
    audioGroup(group),
    audiumEngine(audiumEngine),
    zoomHandler(zoomHandler),
    regionSelector(regionSelector)
{
}

void GroupBaseComponent::paint (juce::Graphics& g)
{
    auto colour = findColour(audium::secondaryBackgroundColourId).brighter();
    if (externalDragAndDrop)
    {
        g.fillAll (colour.withAlpha(0.5f));
    }
    
    if (audioGroup->isSelected())
    {
        g.setColour (juce::Colours::white.withAlpha(0.5f));
        g.drawRoundedRectangle (getLocalBounds().toFloat(), 3.0f, 1.0f);
        
    }
    
}

void GroupBaseComponent::filesDropped (const StringArray& filenames, int x, int y)
{    
    externalDragAndDrop = false;
    regionSelector->setEnabled(true);
    zoomHandler->getSnapToGridHandler()->clearRange();
    repaint();
}

void GroupBaseComponent::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    externalDragAndDrop = true;
    regionSelector->setEnabled(false);
    repaint();
}

void GroupBaseComponent::fileDragMove (const StringArray& files, int x, int y)
{
    auto start = zoomHandler->xToClocks(x);
    auto end = start + 0.01;
    Range<double> rangeInClocks(start, end);
    
    zoomHandler->getSnapToGridHandler()->publishRange(rangeInClocks);
}

void GroupBaseComponent::fileDragExit (const juce::StringArray& files)
{
    externalDragAndDrop = false;
    regionSelector->setEnabled(true);
    zoomHandler->getSnapToGridHandler()->clearRange();
    repaint();
}


void GroupBaseComponent::mouseDown (const MouseEvent& event)
{
    // deselect
    audioGroup->getSelectionManager()->deselectAll();
    
    // pass on mouse events. unless row is not selected
    getParentComponent()->mouseDown(event);
}
