/*
  ==============================================================================

    ArrangementEditBaseComponent.h
    Created: 27 Nov 2023 9:49:52am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/Models/AudioTrackListBoxModel.h"
#include "Interface/Controls/AudioTrackListBox.h"
#include "Util/EngineAccess.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Handlers/SnapToGridHandler.h"
#include "Interface/Controls/PlayPositionMarker.h"
#include "Interface/Controls/TransportPositionControl.h"
#include "Interface/Controls/DragZoomControl.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Interface/AudiumLookAndFeel.h"
#include "Interface/Views/ArrangementOverview.h"
#include "Interface/Views/GridView.h"

//==============================================================================
/*

 Base class to display timeline stuff
 
 This call contains the AudioTrackListBox!

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
        
        // audio track list
        audioTrackListBox.reset(new AudioTrackListBox(audiumEngine, zoomHandler));
        
        // region selector
        regionSelector.reset(new RegionSelector(audioTrackListBox, zoomHandler, audiumEngine));
        addAndMakeVisible(regionSelector.get());
        
        // audio track list model
        audioTrackListBoxModel.reset(new AudioTrackListBoxModel(audioTrackListBox,
                                                                audiumEngine,
                                                                zoomHandler,
                                                                regionSelector,
                                                                arrangementMode));
        audioTrackListBox->setModel(audioTrackListBoxModel.get());
        auto headerComponent = std::unique_ptr<TransportPositionControl> (new TransportPositionControl(audioTrackListBox,
                                                                                                       regionSelector,
                                                                                                       zoomHandler,
                                                                                                       audiumEngine));
        audioTrackListBox->addMouseListener (headerComponent.get(), true);
        audioTrackListBox->setHeaderComponent(std::move(headerComponent));
        audioTrackListBox->getHeaderComponent()->setSize(getWidth(), AudiumLookAndFeel::transportPositionControlHeight);
        audioTrackListBox->setOutlineThickness(0);
        audioTrackListBox->setMultipleSelectionEnabled(true);
        addAndMakeVisible(audioTrackListBox.get());
        
        // drag zoom control
        dragZoomControl.reset(new DragZoomControl(audioTrackListBox, audiumEngine, zoomHandler, arrangementMode));
        addAndMakeVisible(dragZoomControl.get());
        dragZoomControl->addChangeListener(this);
                
        
        // play postition view (on top)
        playPositionMarker.reset(new PlayPositionMarker(zoomHandler, audiumEngine));
        addAndMakeVisible(playPositionMarker.get());

        // view port -> zoom handler
        zoomHandler->setViewport(audioTrackListBox->getViewport());
        
        // grid view
        gridView.reset(new GridView(zoomHandler));
        zoomHandler->getSnapToGridHandler()->addChangeListener(gridView.get());
        audioTrackListBox->getViewport()->getViewedComponent()->addAndMakeVisible(gridView.get());
        
        // background
        gridView->toBack();
        
        // foreground
        regionSelector->toFront(false);
        
        updateUI();
    }

    virtual ~ArrangementEditBaseComponent() override
    {
        audioTrackListBox->removeMouseListener (audioTrackListBox->getHeaderComponent());
        dragZoomControl->removeChangeListener(this);
        zoomHandler->getSnapToGridHandler()->removeChangeListener(gridView.get());
        
        audioTrackListBox->setModel(nullptr);
        audioTrackListBox->setHeaderComponent(nullptr);
        regionSelector = nullptr;
        audioTrackListBox = nullptr;
        audioTrackListBoxModel = nullptr;
        zoomHandler = nullptr;
        playPositionMarker = nullptr;
    }

    void paint (juce::Graphics& g) override
    {
        if (audiumEngine->getAudioResourceContainer()->getNumAudioResources() == 0) {
            g.setColour (juce::Colours::white.withAlpha(0.75f));
            g.setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
            g.drawText ("Drop Audio Files Here", getLocalBounds(),
                        juce::Justification::centred, true);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto top = bounds.removeFromTop(AudiumLookAndFeel::dragZoomControlHeight);
        arrangementOverview->setBounds(top);
        dragZoomControl->setBounds(top);
        audioTrackListBox->setBounds(bounds);
        playPositionMarker->setBounds(bounds);
    }
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        setContentWidth(zoomHandler->getContentWidth());
    }

    void updateUI()
    {
        setContentWidth(zoomHandler->getContentWidth());
        
        audioTrackListBox->updateContent();
        regionSelector->updateFromEngine();
        arrangementOverview->updateFromEngine();
        dragZoomControl->updateFromEngine();
    }
    
    void setContentWidth(int contentWidth)
    {
        audioTrackListBox->setMinimumContentWidth(contentWidth);
        
        // sync our grid view with the Viewport bounds
        jassert(audioTrackListBox->getViewport()->getViewedComponent());
        auto viewPortBounds = audioTrackListBox->getViewport()->getViewedComponent()->getLocalBounds();
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
        auto relativeEvent = event.getEventRelativeTo(audioTrackListBox.get());
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
        return audioTrackListBox->getViewport()->getVerticalScrollBar().getCurrentRange().getStart();
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
    
    std::shared_ptr<AudioTrackListBox>          audioTrackListBox;
    std::unique_ptr<AudioTrackListBoxModel>     audioTrackListBoxModel;
    std::unique_ptr<PlayPositionMarker>         playPositionMarker;
    std::unique_ptr<DragZoomControl>            dragZoomControl;
    std::unique_ptr<ArrangementOverview>        arrangementOverview;
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementEditBaseComponent)
};
