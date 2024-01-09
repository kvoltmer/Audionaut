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
#include "Interface/Controls/DragZoomControl.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Interface/AudiumLookAndFeel.h"
#include "Interface/Views/ArrangementOverview.h"

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

        arrangementOverview.reset(new ArrangementOverview(audiumEngine, arrangementMode));
        addAndMakeVisible(arrangementOverview.get());
        
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
        audioGroupListBox->getHeaderComponent()->setSize(getWidth(), AudiumLookAndFeel::transportPositionControlHeight);
        audioGroupListBox->setOutlineThickness(0);
        
        dragZoomControl.reset(new DragZoomControl(audioGroupListBox, audiumEngine, zoomHandler, arrangementMode));
        addAndMakeVisible(dragZoomControl.get());
        
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
        auto bounds = getLocalBounds();
        auto top = bounds.removeFromTop(AudiumLookAndFeel::dragZoomControlHeight);
        arrangementOverview->setBounds(top);
        dragZoomControl->setBounds(top);
        audioGroupListBox->setBounds(bounds);
        playPositionMarker->setBounds(bounds);
    }

    void updateUI()
    {
        setContentWidth(zoomHandler->getContentWidth());
        
        audioGroupListBox->updateContent();
        regionSelector->updateFromEngine();
        arrangementOverview->updateFromEngine();
        dragZoomControl->updateFromEngine();
    }
    
    void setContentWidth(int contentWidth)
    {
        audioGroupListBox->setMinimumContentWidth(contentWidth);
    }

    void zoomIn()
    {
        zoomHandler->zoomIn();
        setContentWidth(zoomHandler->getContentWidth());
        regionSelector->updateFromEngine();
        dragZoomControl->updateFromEngine();
        zoomHandler->focusViewOnPlayPosition();
    }

    void zoomOut()
    {
        zoomHandler->zoomOut();
        setContentWidth(zoomHandler->getContentWidth());
        regionSelector->updateFromEngine();
        dragZoomControl->updateFromEngine();
        zoomHandler->focusViewOnPlayPosition();
    }
    
    void pageLeft()
    {
        zoomHandler->pageLeft();
    }
    
    void pageRight()
    {
        zoomHandler->pageRight();
    }
    
    void mouseMagnify (const MouseEvent& event, float scaleFactor) override
    {
        auto relativeEvent = event.getEventRelativeTo(audioGroupListBox.get());
        auto x = relativeEvent.getPosition().getX();
        
        // percentage to center of visible range
        auto center = x / zoomHandler->getVisibleRange().getLength();
        
        // absolute position in clocks
        auto clocks = zoomHandler->xToClocksWithOffset(x);
        
        zoomHandler->setZoomFactor(zoomHandler->getZoomFactor() * static_cast<double>(scaleFactor));
        setContentWidth(zoomHandler->getContentWidth());
        regionSelector->updateFromEngine();
        
        zoomHandler->centerView(clocks, center);
    }
    
    RegionSelector* getRegionSelector() const { return regionSelector.get(); }
    
    double getVerticalScrollOffset() const
    {
        return audioGroupListBox->getViewport()->getVerticalScrollBar().getCurrentRange().getStart();
    }
    
    void onScrollContext()
    {
        regionSelector->updateFromEngine();
        dragZoomControl->updateFromEngine();
    }

protected:
    
    std::shared_ptr<AudiumEngine>               audiumEngine;
    std::shared_ptr<ZoomHandler>                zoomHandler;
    std::shared_ptr<RegionSelector>             regionSelector;
    
    std::shared_ptr<AudioGroupListBox>          audioGroupListBox;
    std::unique_ptr<GroupListBoxModel>          audioGroupListBoxModel;
    std::unique_ptr<PlayPositionMarker>         playPositionMarker;
    std::unique_ptr<DragZoomControl>            dragZoomControl;
    std::unique_ptr<ArrangementOverview>        arrangementOverview;
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementEditBaseComponent)
};
