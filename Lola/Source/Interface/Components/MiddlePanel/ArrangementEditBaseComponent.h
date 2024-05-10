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
#include "Interface/Handlers/SnapToGridHandler.h"
#include "Interface/Controls/PlayPositionMarker.h"
#include "Interface/Controls/TransportPositionControl.h"
#include "Interface/Controls/DragZoomControl.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Interface/AudiumLookAndFeel.h"
#include "Interface/Views/ArrangementOverview.h"
#include "Interface/Views/GridView.h"

//==============================================================================
/*

 Base class to display timeline stuff
 
 This call contains the AudioGroupListBox!

 */
class ArrangementEditBaseComponent  : public juce::Component, public juce::ChangeListener
{
public:
    ArrangementEditBaseComponent(std::shared_ptr<AudiumEngine> audiumEngine,
                                 std::shared_ptr<ZoomHandler> zoomHandler,
                                 bool arrangementMode) :
        audiumEngine(audiumEngine),
        zoomHandler(zoomHandler)
    {
        // overview
        arrangementOverview.reset(new ArrangementOverview(audiumEngine, arrangementMode));
        addAndMakeVisible(arrangementOverview.get());
        
        // audio group list
        audioGroupListBox.reset(new AudioGroupListBox(audiumEngine, zoomHandler));
        
        // region selector
        regionSelector.reset(new RegionSelector(audioGroupListBox, zoomHandler, audiumEngine));
        
        
        // audio group list model
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
        audioGroupListBox->setMultipleSelectionEnabled(true);
        audioGroupListBox->setMultipleSelectionEnabled(true);
        addAndMakeVisible(audioGroupListBox.get());
        
        // drag zoom control
        dragZoomControl.reset(new DragZoomControl(audioGroupListBox, audiumEngine, zoomHandler, arrangementMode));
        addAndMakeVisible(dragZoomControl.get());
        dragZoomControl->addChangeListener(this);
        
        // region selector component
        addAndMakeVisible(regionSelector.get());
        
        // play postition view (on top)
        playPositionMarker.reset(new PlayPositionMarker(zoomHandler, audiumEngine));
        addAndMakeVisible(playPositionMarker.get());

        // scroll bar -> zoom handler
        zoomHandler->setHorizontalScrollBar(&audioGroupListBox->getHorizontalScrollBar());
        
        // grid view
        gridView.reset(new GridView(zoomHandler));
        zoomHandler->getSnapToGridHandler()->addChangeListener(gridView.get());
        audioGroupListBox->getViewport()->getViewedComponent()->addAndMakeVisible(gridView.get());
        gridView->toBack();
        
        updateUI();

    }

    virtual ~ArrangementEditBaseComponent() override
    {
        dragZoomControl->removeChangeListener(this);
        zoomHandler->getSnapToGridHandler()->removeChangeListener(gridView.get());
        
        audioGroupListBox->setModel(nullptr);
        audioGroupListBox->setHeaderComponent(nullptr);
        regionSelector = nullptr;
        audioGroupListBox = nullptr;
        audioGroupListBoxModel = nullptr;
        zoomHandler = nullptr;
        playPositionMarker = nullptr;
    }

    void paint (juce::Graphics& g) override
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
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        setContentWidth(zoomHandler->getContentWidth());
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
        
        // sync our grid view with the Viewport bounds
        jassert(audioGroupListBox->getViewport()->getViewedComponent());
        auto viewPortBounds = audioGroupListBox->getViewport()->getViewedComponent()->getLocalBounds();
        viewPortBounds.setWidth(std::max(contentWidth, viewPortBounds.getWidth()));
        gridView->setBounds(viewPortBounds);
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
    std::shared_ptr<GridView>                   gridView;
    
    std::shared_ptr<AudioGroupListBox>          audioGroupListBox;
    std::unique_ptr<GroupListBoxModel>          audioGroupListBoxModel;
    std::unique_ptr<PlayPositionMarker>         playPositionMarker;
    std::unique_ptr<DragZoomControl>            dragZoomControl;
    std::unique_ptr<ArrangementOverview>        arrangementOverview;
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementEditBaseComponent)
};
