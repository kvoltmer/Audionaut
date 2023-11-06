/*
  ==============================================================================

    ArrangementComponent.h
    Created: 23 Oct 2023 12:01:31pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/Models/AudioGroupListBoxModel.h"
#include "Interface/Controls/AudioGroupListBox.h"
#include "Util/EngineAccess.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Controls/PlayPositionMarker.h"
#include "Interface/Controls/TransportPositionControl.h"
#include "Engine/AudioGroupContainer.h"

class ArrangementComponent  : public juce::Component
{
public:
    ArrangementComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
        audiumEngine(audiumEngine)
    {
        
        zoomHandler.reset(new ZoomHandler(audiumEngine->getAudioResourceContainer(),
                                          audiumEngine->getPlayListScheduler()));
        audioGroupListBox.reset(new AudioGroupListBox(audiumEngine, "Audio Group Listbox", nullptr));
        regionSelector.reset(new RegionSelector(audioGroupListBox, zoomHandler, audiumEngine));
        audioGroupListBoxModel.reset(new AudioGroupListBoxModel(audioGroupListBox,
                                                                audiumEngine,
                                                                zoomHandler));
        audioGroupListBox->setModel(audioGroupListBoxModel.get());
        auto headerComponent = std::unique_ptr<TransportPositionControl> (new TransportPositionControl(audioGroupListBox, zoomHandler, audiumEngine));
        audioGroupListBox->setHeaderComponent(std::move(headerComponent));
        audioGroupListBox->getHeaderComponent()->setSize(getWidth(), 25);
        audioGroupListBox->setOutlineThickness(0);
        
        
        playPositionMarker.reset(new PlayPositionMarker(zoomHandler, audiumEngine));
        

        zoomHandler->setHorizontalScrollBar(&audioGroupListBox->getHorizontalScrollBar());


        audioGroupListBox->setMultipleSelectionEnabled(true);
        addAndMakeVisible(audioGroupListBox.get());

        // selection component
        addAndMakeVisible(regionSelector.get());
        
        // make sure the play postition view is on top
        addAndMakeVisible(playPositionMarker.get());

        updateUI();

    }

    ~ArrangementComponent() override
    {
        audioGroupListBox->setHeaderComponent(nullptr);
        audioGroupListBox->setModel(nullptr);
        regionSelector = nullptr;
        audioGroupListBox = nullptr;
        audioGroupListBoxModel = nullptr;
        zoomHandler = nullptr;
        playPositionMarker = nullptr;
    }

    void paint (juce::Graphics&) override
    {
    }

    void resized() override
    {
        audioGroupListBox->setBounds(getLocalBounds());
        playPositionMarker->setBounds(getLocalBounds());
    }

    void updateUI()
    {
        setContentWidth(zoomHandler->getArrangementContentWidth());
        
        audioGroupListBox->updateContent();
        regionSelector->updateFromEngine();
    }
    
    void setContentWidth(int contentWidth)
    {
        audioGroupListBox->setMinimumContentWidth(contentWidth);
    }

    void zoomIn()
    {
        zoomHandler->zoomIn();
        auto width = zoomHandler->getArrangementContentWidth();
        //std::cout << "ArrangementComponent width " << width << std::endl;
        setContentWidth(width);
        regionSelector->updateFromEngine();
        zoomHandler->focusViewOnPlayPosition();
        
        updateUI();
    }

    void zoomOut()
    {
        zoomHandler->zoomOut();
        auto width = zoomHandler->getArrangementContentWidth();
        //std::cout << "ArrangementComponent width " << width << std::endl;
        setContentWidth(width);
        regionSelector->updateFromEngine();
        zoomHandler->focusViewOnPlayPosition();
        
        updateUI();
    }

private:
    
    std::shared_ptr<AudiumEngine>               audiumEngine;
    std::shared_ptr<ZoomHandler>                zoomHandler;
    std::shared_ptr<RegionSelector>             regionSelector;
    std::shared_ptr<AudioGroupListBox>       audioGroupListBox;
    std::shared_ptr<AudioGroupListBoxModel>  audioGroupListBoxModel;
    std::shared_ptr<PlayPositionMarker>         playPositionMarker;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementComponent)
};
