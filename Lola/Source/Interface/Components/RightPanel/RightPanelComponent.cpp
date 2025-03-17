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


#include <JuceHeader.h>
#include "RightPanelComponent.h"
#include "Engine/AudiumEngine.h"
#include "Interface/ColourIds.h"
#include "Interface/Components/RightPanel/PlayListComponent.h"
#include "Interface/Components/RightPanel/PlayListContainerComponent.h"
#include "Interface/Components/RightPanel/RegionComponent.h"
#include "Interface/Components/RightPanel/RegionContainerComponent.h"

RightPanelComponent::RightPanelComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
    audiumEngine(audiumEngine)
{
    regionComponent.reset(new RegionComponent(audiumEngine));
    stretchableLayoutManager.reset(new juce::StretchableLayoutManager());
    stretchableLayoutResizerBar.reset(new juce::StretchableLayoutResizerBar(stretchableLayoutManager.get(), 1, false));
    playListContainerComponent.reset(new PlayListContainerComponent(audiumEngine));
    regionContainerComponent.reset(new RegionContainerComponent(audiumEngine));

    addAndMakeVisible(regionComponent.get());
    addAndMakeVisible(stretchableLayoutResizerBar.get());
    addAndMakeVisible(playListContainerComponent.get());
    addAndMakeVisible(regionContainerComponent.get());
    
    stretchableLayoutManager->setItemLayout (0,          // for item 0
                                             25, -1.0,    // size must be between 0% and 100% of the available space
                                             -0.5);      // and its preferred size in % of the total available space

    stretchableLayoutManager->setItemLayout (1, // for item 1
                                             3, 3, 3);

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
    regionContainerComponent->setVisible(isArrangement);
    
    regionComponent->setVisible(!isArrangement);
    
    if (isArrangement) {
        // the list of components that we want to reposition
        Component* comps[] = {  regionContainerComponent.get(),
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
    regionContainerComponent->updateUI(context);
    
    // obsolete?
    resized();
    
}
