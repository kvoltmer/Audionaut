//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "../JuceLibraryCode/BinaryData.h"
#include "Engine/AudiumEngine.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Controls/AudioTrackListBox.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListItem.h"

class DragZoomControl  : public juce::Component, public juce::ChangeBroadcaster
{
public:
    DragZoomControl(std::shared_ptr<AudioTrackListBox> audioTrackListBox,
                    std::shared_ptr<audium::AudiumEngine> audiumEngine,
                    std::shared_ptr<ZoomHandler> zoomHandler,
                    bool arrangementMode) :
        audioTrackListBox(audioTrackListBox),
        audiumEngine(audiumEngine),
        zoomHandler(zoomHandler),
        arrangementMode(arrangementMode)
    {
        visibileRectangle = std::make_unique<juce::DrawableRectangle>();
        addAndMakeVisible(visibileRectangle.get());
        visibileRectangle->setFill (juce::Colours::transparentBlack);
        visibileRectangle->setStrokeFill (juce::Colours::white);
        visibileRectangle->setStrokeThickness(1.f);
        updateFromEngine();
        

        // load svg and apply zoom cursor
        drawable = juce::Drawable::createFromImageData (BinaryData::zoomininv_svg, BinaryData::zoomininv_svgSize);
        juce::Rectangle<int> rect(0, 0, 17, 17);
        cursorImage = juce::Image(Image::PixelFormat::ARGB, rect.getWidth(), rect.getHeight(), true);
        auto g = juce::Graphics(cursorImage);
        drawable->drawWithin(g, rect.toFloat(), RectanglePlacement::centred | RectanglePlacement::onlyReduceInSize, 1.f);
        zoomCursor = juce::MouseCursor(cursorImage, rect.getWidth()/2, rect.getHeight()/2);
        
    }

    ~DragZoomControl() override
    {
    }

    void paint (juce::Graphics& g) override
    {

    }
    

    void resized() override
    {
        updateFromEngine();
    }
    
    void updateFromEngine()
    {
        auto bounds = getLocalBounds().toFloat();
        
        auto contentWidth = zoomHandler->getContentWidth();
        jassert(contentWidth > 0.0);
        auto visibleRange = zoomHandler->getVisibleRange();
        
        auto x = (visibleRange.getStart() / contentWidth) * bounds.getWidth();
        auto y = bounds.getY();
        auto w = (visibleRange.getLength() / contentWidth) * bounds.getWidth();
        
        w = std::min((float)w, bounds.getWidth());
        w = std::max((float)w, 10.f);
        
        auto h = bounds.getHeight();
        
        visibileRectangle->setRectangle(juce::Rectangle<float>(x, y, w, h).reduced(1.f, 2.f));
    }
    
    
    
    void mouseDown (const juce::MouseEvent& e) override
    {
        // set follow transport off
        followTransportWasActive = audiumEngine->getPlayListScheduler()->getFollowTransport();
        audiumEngine->getPlayListScheduler()->setFollowTransport(false);
        
        mouseDownFactor = zoomHandler->getZoomFactor();
        mouseDrag(e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        // zoom in - zoom out
        auto newFactor = mouseDownFactor;
        auto f = 0.0;
        auto y = static_cast<double>(e.getOffsetFromDragStart().getY());
        if (std::abs(y) >= 2.0)
        {
            if (y > 0.0)
            {
                // eg: with 50 pixel distance the zoom will double
                f = std::pow(2.0, y * 0.02);
            }
            else
            {
                f = std::pow(0.5, std::abs(y) * 0.02);
            }
            
            newFactor = mouseDownFactor * f;
            //std::cout << "y:" << y << " f: " << f << " " << newFactor << std::endl;
        }
        
        zoomHandler->setZoomFactor(newFactor);

        // where the visible range should end up centred, as a fraction of the
        // content width - the change listener applies it after resizing the
        // content, since the centring needs the new content width in place
        targetCenterFraction = e.position.getX() / getLocalBounds().toFloat().getWidth();

        // asynchronous and coalesced: a pending change message swallows
        // further ones, so a burst of drag events causes one content rebuild
        // per message-loop pass instead of one per event
        sendChangeMessage();
    }

    double getTargetCenterFraction() const noexcept { return targetCenterFraction; }

    void mouseUp (const juce::MouseEvent& e) override
    {
        updateFromEngine();
        
        // reactivate
        audiumEngine->getPlayListScheduler()->setFollowTransport(followTransportWasActive);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (zoomCursor);
    }

private:
    std::unique_ptr<juce::DrawableRectangle>    visibileRectangle;
    std::shared_ptr<AudioTrackListBox>          audioTrackListBox;
    std::shared_ptr<audium::AudiumEngine>               audiumEngine;
    std::shared_ptr<ZoomHandler>                zoomHandler;
    bool                                        arrangementMode;
    
    double mouseDownFactor = 1.0;

    double targetCenterFraction = 0.5;
    
    // cursor stuff:
    juce::MouseCursor zoomCursor;
    std::unique_ptr<juce::Drawable> drawable;
    juce::Image cursorImage;
    
    // follow transport conflics with the drag zoom control
    bool followTransportWasActive;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DragZoomControl)
};
