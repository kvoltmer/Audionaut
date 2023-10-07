/*
  ==============================================================================

    MiddlePanelComponent.cpp
    Created: 6 Jun 2023 11:51:48am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "MiddlePanelComponent.h"
#include "Interface/Models/AudioGroupListBoxModel.h"
#include "Interface/Controls/AudioGroupListBox.h"
#include "Util/EngineAccess.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Controls/PlayPositionMarker.h"
#include "Interface/Controls/TransportPositionControl.h"

//==============================================================================
MiddlePanelComponent::MiddlePanelComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
    audiumEngine(audiumEngine)
{
    /// TODO: handle this in a more elegant way
    auto initialWidth = 1000;
    
    zoomHandler.reset(new ZoomHandler(audiumEngine->getAudioResourceContainer(),
                                      audiumEngine->getPlayListScheduler()));
    audioGroupListBox.reset(new AudioGroupListBox(audiumEngine, "Audio Group Listbox", nullptr));
    regionSelector.reset(new RegionSelector(audioGroupListBox, zoomHandler, audiumEngine));
    audioGroupListBoxModel.reset(new AudioGroupListBoxModel(audioGroupListBox,
                                                            audiumEngine,
                                                                  audiumEngine->getAudioResourceContainer(),
                                                                  audiumEngine->getPlayListContainer(),
                                                                  zoomHandler,
                                                                  regionSelector));
    audioGroupListBox->setModel(audioGroupListBoxModel.get());
    auto headerComponent = std::unique_ptr<TransportPositionControl> (new TransportPositionControl(audioGroupListBox, zoomHandler, audiumEngine));
    audioGroupListBox->setHeaderComponent(std::move(headerComponent));
    audioGroupListBox->getHeaderComponent()->setSize(initialWidth, 25);
    audioGroupListBox->setMinimumContentWidth(initialWidth);
    audioGroupListBox->setOutlineThickness(0);
    
    playPositionMarker.reset(new PlayPositionMarker(zoomHandler, audiumEngine));
    addAndMakeVisible(playPositionMarker.get());

    zoomHandler->setHorizontalScrollBar(&audioGroupListBox->getHorizontalScrollBar());


    audioGroupListBox->setMultipleSelectionEnabled(true);
    addAndMakeVisible(audioGroupListBox.get());

    // selection component
    addAndMakeVisible(regionSelector.get());
    
    
    
    zoomHandler->setWidth(initialWidth);
    
    // make sure the play postition view is on top 
    playPositionMarker->toFront(false);

    updateUI();

}

MiddlePanelComponent::~MiddlePanelComponent()
{
    audioGroupListBox->setHeaderComponent(nullptr);
    audioGroupListBox->setModel(nullptr);
    regionSelector = nullptr;
    audioGroupListBox = nullptr;
    audioGroupListBoxModel = nullptr;
    zoomHandler = nullptr;
    playPositionMarker = nullptr;
}

void MiddlePanelComponent::resized()
{
    audioGroupListBox->setBounds(getLocalBounds());
    playPositionMarker->setBounds(getLocalBounds());
}

void MiddlePanelComponent::updateUI()
{
    audioGroupListBox->updateContent();
    regionSelector->updateFromEngine();
}

void MiddlePanelComponent::zoomIn()
{
    auto width = getWidth() * zoomHandler->zoomIn();
    audioGroupListBox->setMinimumContentWidth(width);
    zoomHandler->setWidth(width);
    regionSelector->updateFromEngine();
    zoomHandler->focusViewOnPlayPosition();
    
    updateUI();
}

void MiddlePanelComponent::zoomOut()
{
    auto width = getWidth() * zoomHandler->zoomOut();
    audioGroupListBox->setMinimumContentWidth(width);
    zoomHandler->setWidth(width);
    regionSelector->updateFromEngine();
    zoomHandler->focusViewOnPlayPosition();
    
    updateUI();
}
