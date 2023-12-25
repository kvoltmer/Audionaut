/*
  ==============================================================================

    ArrangementEditBaseComponent.h
    Created: 27 Nov 2023 9:49:52am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/Models/GroupListBoxModel.h"
#include "Interface/Controls/AudioGroupListBox.h"
#include "Util/EngineAccess.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Controls/PlayPositionMarker.h"
#include "Interface/Controls/TransportPositionControl.h"
#include "Engine/Group/AudioGroupContainer.h"

//==============================================================================
/*

 Base class to display timeline stuff
 
 This call contains the AudioGroupListBox!

 */
class ArrangementEditBaseComponent  : public juce::Component//, public juce::KeyListener
{
public:
    ArrangementEditBaseComponent(std::shared_ptr<AudiumEngine> audiumEngine, bool arrangementMode) :
        audiumEngine(audiumEngine)
    {
        
        zoomHandler.reset(new ZoomHandler(audiumEngine->getPlayListScheduler()));
        audioGroupListBox.reset(new AudioGroupListBox(audiumEngine, zoomHandler));
        regionSelector.reset(new RegionSelector(audioGroupListBox, zoomHandler, audiumEngine));
        audioGroupListBoxModel.reset(new GroupListBoxModel(audioGroupListBox,
                                                                audiumEngine,
                                                                zoomHandler,
                                                                regionSelector,
                                                                arrangementMode));
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

    virtual ~ArrangementEditBaseComponent() override
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
        setContentWidth(zoomHandler->getContentWidth());
        
        audioGroupListBox->updateContent();
        regionSelector->updateFromEngine();
        
    }
    
    void setContentWidth(int contentWidth)
    {
        audioGroupListBox->setMinimumContentWidth(contentWidth);
    }

    void zoomIn()
    {
        auto centerInSeconds = zoomHandler->getVisibleRangeInSeconds().getStart() + (zoomHandler->getVisibleRangeInSeconds().getLength() * 0.5);
        zoomHandler->zoomIn();
        setContentWidth(zoomHandler->getContentWidth());
        regionSelector->updateFromEngine();
        
        auto regionSelectorPos = audiumEngine->getAudioRegionContainer()->getSelectedPositionInSeconds();
        if (!regionSelectorPos.isEmpty())
        {
            centerInSeconds = regionSelectorPos.getStart() + (regionSelectorPos.getLength() * 0.5);
        }
        
        zoomHandler->focusView(centerInSeconds);
    }

    void zoomOut()
    {
        auto centerInSeconds = zoomHandler->getVisibleRangeInSeconds().getStart() + (zoomHandler->getVisibleRangeInSeconds().getLength() * 0.5);        zoomHandler->zoomOut();
        setContentWidth(zoomHandler->getContentWidth());
        regionSelector->updateFromEngine();
        zoomHandler->focusView(centerInSeconds);
    }
    
    RegionSelector* getRegionSelector() const { return regionSelector.get(); }

protected:
    
    std::shared_ptr<AudiumEngine>               audiumEngine;
    std::shared_ptr<ZoomHandler>                zoomHandler;
    std::shared_ptr<RegionSelector>             regionSelector;
    std::shared_ptr<AudioGroupListBox>          audioGroupListBox;
    std::shared_ptr<GroupListBoxModel>     audioGroupListBoxModel;
    std::shared_ptr<PlayPositionMarker>         playPositionMarker;
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementEditBaseComponent)
};
