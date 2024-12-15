/*
  ==============================================================================

    RightPanelComponent.cpp
    Created: 6 Jun 2023 11:50:49am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "RightPanelComponent.h"
#include "Engine/AudiumEngine.h"
#include "Interface/ColourIds.h"
#include "Interface/Components/RightPanel/PlayListComponent.h"
#include "Interface/Components/RightPanel/PlayListContainerComponent.h"
#include "Interface/Components/RightPanel/RegionComponent.h"
#include "Interface/Components/RightPanel/RegionsPerTrackContainerComponent.h"

//==============================================================================
RightPanelComponent::RightPanelComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
    audiumEngine(audiumEngine)
{
    regionComponent.reset(new RegionComponent(audiumEngine));
    stretchableLayoutManager.reset(new juce::StretchableLayoutManager());
    stretchableLayoutResizerBar.reset(new juce::StretchableLayoutResizerBar(stretchableLayoutManager.get(), 1, false));
    playListContainerComponent.reset(new PlayListContainerComponent(audiumEngine));
    
    regionsPerTrackContainer.reset(new RegionsPerTrackContainerComponent(audiumEngine));
    

    addAndMakeVisible(regionComponent.get());
    addAndMakeVisible(stretchableLayoutResizerBar.get());
    addAndMakeVisible(playListContainerComponent.get());
    addAndMakeVisible(regionsPerTrackContainer.get());
    
    stretchableLayoutManager->setItemLayout (0,          // for item 0
                                             25, -1.0,    // size must be between 0% and 100% of the available space
                                             -0.5);      // and its preferred size in % of the total available space

    stretchableLayoutManager->setItemLayout (1, // for item 1
                                             2, 2, 2);

    stretchableLayoutManager->setItemLayout (2,          // for item 2
                                             25, -1.0, // size must be between 0% and 50% of the available space
                                             -0.5);        // its preferred size in pixels

    resized();
}

RightPanelComponent::~RightPanelComponent()
{

}

void RightPanelComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void RightPanelComponent::resized()
{
    auto isArrangement = audiumEngine->getPlayListScheduler()->isArrangementMode();
    
    playListContainerComponent->setVisible(isArrangement);
    regionsPerTrackContainer->setVisible(isArrangement);
    regionComponent->setVisible(!isArrangement);
    
    if (isArrangement) {
        // the list of components that we want to reposition
        Component* comps[] = {  regionsPerTrackContainer.get(),
            stretchableLayoutResizerBar.get(),
            playListContainerComponent.get() };
        
        // this will position the 3 components, one above the other, to fit
        // horizontically into the rectangle provided.
        stretchableLayoutManager->layOutComponents (comps, 3,
                                                    0, 0, getWidth(), getHeight(),
                                                    true, true);
    }
    else {
        regionComponent->setBounds (getLocalBounds());
    }
}

void RightPanelComponent::updateUI(UIContext context)
{
    regionComponent->updateUI(context);
    playListContainerComponent->updateUI(context);
    regionsPerTrackContainer->updateUI(context);
    
    // obsolete?
    resized();
    
}

void RightPanelComponent::clearSelection()
{
    /// TODO: implement
    //regionsPerTrackContainer->clearSelection();
    regionComponent->clearSelection();
}
