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
#include "Interface/Controls/PositionMarker.h"
#include "Interface/Controls/TransportPositionControl.h"
#include "Interface/Controls/DragZoomControl.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Interface/Views/ArrangementOverview.h"
#include "Interface/Views/GridView.h"

//==============================================================================
/*

 Base class to display timeline stuff
 
 This class contains the AudioTrackListBox!

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
        // arragnement overview
        arrangementOverview = std::make_unique<ArrangementOverview> (audiumEngine, arrangementMode);
        addAndMakeVisible(arrangementOverview.get());
        
        // drag zoom control
        dragZoomControl = std::make_unique<DragZoomControl> (audioTrackListBox, audiumEngine, zoomHandler, arrangementMode);
        addAndMakeVisible(dragZoomControl.get());
        dragZoomControl->addChangeListener(this);
        
            
        // audio track list
        audioTrackListBox = std::make_unique<AudioTrackListBox> (audiumEngine, zoomHandler);
        
        // region selector
        regionSelector = std::make_shared<RegionSelector> (audioTrackListBox, zoomHandler, audiumEngine);
        addAndMakeVisible(regionSelector.get());
        
        // audio track list model
        audioTrackListBoxModel = std::make_unique<AudioTrackListBoxModel>(audioTrackListBox,
                                                                          audiumEngine,
                                                                          zoomHandler,
                                                                          regionSelector,
                                                                          arrangementMode);
        audioTrackListBox->setModel(audioTrackListBoxModel.get());
        
        // transport position control
        auto headerComponent = std::make_unique<TransportPositionControl> (audioTrackListBox,
                                                                           regionSelector,
                                                                           zoomHandler,
                                                                           audiumEngine);
        audioTrackListBox->addMouseListener (headerComponent.get(), true);
        audioTrackListBox->setHeaderComponent(std::move(headerComponent));
        audioTrackListBox->getHeaderComponent()->setSize(getWidth(), AudiumLookAndFeel::transportPositionControlHeight);
        audioTrackListBox->setOutlineThickness(0);
        audioTrackListBox->setMultipleSelectionEnabled(true);
        addAndMakeVisible(audioTrackListBox.get());
        
        // view port -> zoom handler
        zoomHandler->setViewport(audioTrackListBox->getViewport());
        
        // grid view (attached to viewport of AudioTrackListBox)
        gridView = std::make_unique<GridView> (zoomHandler);
        zoomHandler->getSnapToGridHandler()->addChangeListener(gridView.get());
        audioTrackListBox->getViewport()->getViewedComponent()->addAndMakeVisible(gridView.get());
        gridView->toBack();
     
        // loop start postition view
        loopStartPositionMarker = std::make_unique<PositionMarker>(zoomHandler);
        addAndMakeVisible(loopStartPositionMarker.get());
        loopStartPositionMarker->setColour(Colours::white.withAlpha (0.5f));
        loopStartPositionMarker->onUpdatePosition = [audiumEngine] (auto context) {
            return audiumEngine->getPlayListScheduler()->getLoopPositionRange(context).getStart();
        };
        
        // loop end postition view
        loopEndPositionMarker = std::make_unique<PositionMarker>(zoomHandler);
        addAndMakeVisible(loopEndPositionMarker.get());
        loopEndPositionMarker->setColour(Colours::white.withAlpha (0.5f));
        loopEndPositionMarker->onUpdatePosition = [audiumEngine] (auto context) {
            return audiumEngine->getPlayListScheduler()->getLoopPositionRange(context).getEnd();
        };
        
        // play postition view (on top)
        playPositionMarker = std::make_unique<PositionMarker>(zoomHandler);
        addAndMakeVisible(playPositionMarker.get());
        playPositionMarker->setColour(Colours::white.withAlpha (0.85f));
        playPositionMarker->onUpdatePosition = [audiumEngine] (auto context) {
            return audiumEngine->getPlayListScheduler()->getAbsolutePosition(context);
        };

        
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
        loopStartPositionMarker->setBounds(bounds);
        loopEndPositionMarker->setBounds(bounds);
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
        
        loopStartPositionMarker->setVisible(audiumEngine->getPlayListScheduler()->isLoopActive());
        loopEndPositionMarker->setVisible(audiumEngine->getPlayListScheduler()->isLoopActive());
        
        audioTrackListBox->getHeaderComponent()->resized();
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
    
    std::unique_ptr<GridView>                   gridView;
    std::shared_ptr<AudioTrackListBox>          audioTrackListBox;
    std::unique_ptr<AudioTrackListBoxModel>     audioTrackListBoxModel;
    std::unique_ptr<PositionMarker>             playPositionMarker;
    std::unique_ptr<PositionMarker>             loopStartPositionMarker;
    std::unique_ptr<PositionMarker>             loopEndPositionMarker;
    std::unique_ptr<DragZoomControl>            dragZoomControl;
    std::unique_ptr<ArrangementOverview>        arrangementOverview;
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementEditBaseComponent)
};
