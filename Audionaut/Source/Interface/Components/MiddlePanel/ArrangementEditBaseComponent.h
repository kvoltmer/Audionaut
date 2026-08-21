//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <optional>
#include <utility>

#include <JuceHeader.h>

#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/TransportLoop.h"
#include "Util/EngineAccess.h"

#include "Interface/Models/AudioTrackListBoxModel.h"
#include "Interface/Controls/AudioTrackListBox.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Handlers/SnapToGridHandler.h"
#include "Interface/Controls/PositionMarker.h"
#include "Interface/Controls/TransportPositionControl.h"
#include "Interface/Controls/DragZoomControl.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Interface/Views/ArrangementOverview.h"
#include "Interface/Views/GridView.h"

//==============================================================================
/*

 Base class to display timeline stuff
 
 This class contains the AudioTrackListBox!

 */
class ArrangementEditBaseComponent  : public juce::Component, public juce::ChangeListener, private juce::AsyncUpdater
{
public:
    ArrangementEditBaseComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine,
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
     

        // loop postition view
        auto transportLoop = audiumEngine->getPlayListScheduler()->getTransportLoop();
        
        loopRangeMarker = std::make_unique<PositionMarker>(zoomHandler);
        addAndMakeVisible(loopRangeMarker.get());
        loopRangeMarker->setColour(Colours::black.withAlpha (0.2f));
        loopRangeMarker->onUpdateRange = [transportLoop] (auto context) {
            return transportLoop->getLoopPositionRange(context);
        };

        
        // play postition view (on top)
        playPositionMarker = std::make_unique<PositionMarker>(zoomHandler);
        addAndMakeVisible(playPositionMarker.get());
        playPositionMarker->setColour(Colours::white.withAlpha (0.5f));
        playPositionMarker->onUpdatePosition = [audiumEngine] (auto context) {
            return audiumEngine->getPlayListScheduler()->getAbsolutePosition(context);
        };

        
        // foreground
        regionSelector->toFront(false);
        
        updateUI();
    }

    virtual ~ArrangementEditBaseComponent() override
    {
        cancelPendingUpdate();
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
            auto txt = "Drop Audio Files Here";
#ifdef JUCE_LINUX
            txt = "Open the file browser (VIEW -> Open File Browser) and drop audio files here.";
#endif
            g.drawText (txt, getLocalBounds(),
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
        
        loopRangeMarker->setBounds(bounds);
    }
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        // the drag-zoom factor is applied HERE, atomically with the resize
        // and re-centring below - never mid-gesture, where an interleaved
        // paint would draw the grid and ruler with new zoom but old scroll
        if (source == dragZoomControl.get())
            zoomHandler->setZoomFactor(dragZoomControl->getTargetZoomFactor());

        setContentWidth(zoomHandler->getContentWidth());

        if (source == dragZoomControl.get())
        {
            // centre the visible range where the drag-zoom gesture points;
            // this has to follow setContentWidth so the scrollbar's total
            // range already reflects the new zoom factor
            auto contentPosition = zoomHandler->getContentWidth() * dragZoomControl->getTargetCenterFraction();
            auto visibleRange = zoomHandler->getVisibleRange();
            zoomHandler->setVisibleRange(visibleRange.movedToStartAt(contentPosition - visibleRange.getLength() * 0.5),
                                         juce::sendNotificationSync);
            dragZoomControl->updateFromEngine();

            // last: the selector's position depends on the scroll offset the
            // setVisibleRange() above just changed
            regionSelector->updateFromEngine();
        }
    }

    void updateUI()
    {
        // will call -> audioTrackListBox->updateContent()
        setContentWidth(zoomHandler->getContentWidth());

        // mirror the engine's track selection into the list box rows, so
        // modifier-key clicks build on selections made in other views
        audioTrackListBox->setSelectedRows(audiumEngine->getAudioTrackContainer()->getSelectedRows(),
                                           juce::dontSendNotification);


        regionSelector->updateFromEngine();
        arrangementOverview->updateFromEngine();
        dragZoomControl->updateFromEngine();


        auto loopActive = audiumEngine->getPlayListScheduler()->getTransportLoop()->isLoopActive();
        loopRangeMarker->setVisible(loopActive);
        
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

        // the grid's and the ruler's spacing depend on the zoom factor, so
        // they need a repaint even when their bounds did not change (the
        // header's timer only moves its markers, it never repaints the ruler)
        gridView->repaint();

        audioTrackListBox->getHeaderComponent()->resized();
        audioTrackListBox->getHeaderComponent()->repaint();
    }

    void zoomIn()
    {
        zoomHandler->zoomIn();
        pendingFocusPlayPosition = true;
        triggerAsyncUpdate();
    }

    void zoomOut()
    {
        zoomHandler->zoomOut();
        pendingFocusPlayPosition = true;
        triggerAsyncUpdate();
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
        pendingZoomCenter = std::make_pair(clocks, center);
        triggerAsyncUpdate();
    }
    
    RegionSelector* getRegionSelector() const { return regionSelector.get(); }
    
    double getVerticalScrollOffset() const
    {
        return audioTrackListBox->getViewport()->getVerticalScrollBar().getCurrentRange().getStart();
    }
    
    void setVerticalScrollOffset(double offset)
    {
        // std::cout << "setVerticalScrollOffset " << offset << std::endl;
        auto range = audioTrackListBox->getViewport()->getVerticalScrollBar().getCurrentRange().withStart(offset);
        audioTrackListBox->getViewport()->getVerticalScrollBar().setCurrentRange(range, sendNotificationSync);
    }
    
    double getHorizontalScrollOffset() const
    {
        return audioTrackListBox->getViewport()->getHorizontalScrollBar().getCurrentRange().getStart();
    }
    
    void setHorizontalScrollOffset(double offset)
    {
        // std::cout << "setHorizontalScrollOffset " << offset << std::endl;
        auto range = audioTrackListBox->getViewport()->getHorizontalScrollBar().getCurrentRange().withStart(offset);
        audioTrackListBox->getViewport()->getHorizontalScrollBar().setCurrentRange(range, sendNotificationSync);
    }
    
    void onScrollContext()
    {
        regionSelector->updateFromEngine();
        dragZoomControl->updateFromEngine();
    }

protected:
    
    std::shared_ptr<audium::AudiumEngine>       audiumEngine;
    std::shared_ptr<ZoomHandler>                zoomHandler;
    std::shared_ptr<RegionSelector>             regionSelector;
    
    std::unique_ptr<GridView>                   gridView;
    std::shared_ptr<AudioTrackListBox>          audioTrackListBox;
    std::unique_ptr<AudioTrackListBoxModel>     audioTrackListBoxModel;
    std::unique_ptr<PositionMarker>             playPositionMarker;
    std::unique_ptr<PositionMarker>             loopRangeMarker;

    std::unique_ptr<DragZoomControl>            dragZoomControl;
    std::unique_ptr<ArrangementOverview>        arrangementOverview;

private:

    // Zoom gestures only store the new zoom factor and trigger this deferred
    // update, so a burst of pinch/key events queued behind a slow rebuild
    // collapses into a single rebuild instead of one per event.
    void handleAsyncUpdate() override
    {
        setContentWidth(zoomHandler->getContentWidth());
        dragZoomControl->updateFromEngine();

        if (pendingZoomCenter.has_value())
        {
            zoomHandler->centerView(pendingZoomCenter->first, pendingZoomCenter->second);
            pendingZoomCenter.reset();
        }

        if (pendingFocusPlayPosition)
        {
            pendingFocusPlayPosition = false;
            zoomHandler->focusViewOnPlayPosition();
        }

        // last: the selector's position depends on the scroll offset the
        // centring/focus calls above may have changed
        regionSelector->updateFromEngine();
    }

    // position (clocks) to keep under the pinch gesture, and where in the
    // visible range (fraction) to keep it
    std::optional<std::pair<double, double>>    pendingZoomCenter;

    bool                                        pendingFocusPlayPosition = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementEditBaseComponent)
};
