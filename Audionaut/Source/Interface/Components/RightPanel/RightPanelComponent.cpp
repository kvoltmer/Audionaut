//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.


#include <JuceHeader.h>
#include "RightPanelComponent.h"
#include "Engine/AudiumEngine.h"
#include "Interface/ColourIds.h"
#include "Interface/Components/RightPanel/PlayListComponent.h"
#include "Interface/Components/RightPanel/PlayListContainerComponent.h"
#include "Interface/Components/RightPanel/RegionComponent.h"
#include "Interface/Components/RightPanel/RegionContainerComponent.h"
#include "Interface/Components/RightPanel/FooterComponent.h"

RightPanelComponent::RightPanelComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine) :
    audiumEngine(audiumEngine)
{

    playListContainerComponent.reset(new PlayListContainerComponent(audiumEngine));
    regionComponent.reset(new RegionComponent(audiumEngine));
    stretchableLayoutManager.reset(new juce::StretchableLayoutManager());
    stretchableLayoutResizerBar.reset(new juce::StretchableLayoutResizerBar(stretchableLayoutManager.get(), 1, false));
    footerComponent.reset(new FooterComponent(audiumEngine));
    
    addAndMakeVisible(regionComponent.get());
    addAndMakeVisible(stretchableLayoutResizerBar.get());
    addAndMakeVisible(playListContainerComponent.get());
    addAndMakeVisible(footerComponent.get());
    
    auto footerHeight = AudiumLookAndFeel::tableHeaderHeight;
    
    stretchableLayoutManager->setItemLayout (0,          // for item 0
                                             25, -1.0,    // size must be between 0% and 100% of the available space
                                             -0.5);      // and its preferred size in % of the total available space

    stretchableLayoutManager->setItemLayout (1, // for item 1
                                             3, 3, 3);

    stretchableLayoutManager->setItemLayout (2,          // for item 2
                                             25, -1.0, // size must be between 0% and 50% of the available space
                                             -0.5);        // its preferred size in pixels
    
    stretchableLayoutManager->setItemLayout (3, // for item 3
                                             footerHeight, footerHeight, footerHeight);

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

    // the list of components that we want to reposition
    Component* comps[] = {  playListContainerComponent.get(),
                            stretchableLayoutResizerBar.get(),
                            regionComponent.get(),
                            footerComponent.get()
                             };

    // this will position the 3 components, one above the other, to fit
    // horizontically into the rectangle provided.
    stretchableLayoutManager->layOutComponents (comps, 4,
                                                0, 0, getWidth(), getHeight(),
                                                true, true);

}

void RightPanelComponent::updateUI(UIContext context)
{
    playListContainerComponent->updateUI(context);
    regionComponent->updateUI(context);
    
    resized();
}
