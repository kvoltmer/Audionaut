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
        drawable = juce::Drawable::createFromImageData (BinaryData::zoomin_svg, BinaryData::zoomin_svgSize);
        juce::Rectangle<int> rect(0, 0, 17, 17);
        cursorImage = juce::Image(Image::PixelFormat::RGB, rect.getWidth(), rect.getHeight(), true);
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
        
        visibileRectangle->setRectangle(Rectangle<float>(x, y, w, h).reduced(1.f, 2.f));
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
        
        // the change message will trigger ArrangementEditBaseComponent::setContentWidth
        sendSynchronousChangeMessage();
        
        // left - right
        auto x = e.position.getX();
        auto contentWidth = zoomHandler->getContentWidth();
        auto w = getLocalBounds().toFloat().getWidth();
        
        auto relativePosition = x / w;
        
        auto contentPosition = contentWidth * relativePosition;
        auto visibleRange = zoomHandler->getVisibleRange();
        zoomHandler->setVisibleRange(visibleRange.movedToStartAt(contentPosition - visibleRange.getLength() * 0.5),
                                     sendNotificationAsync);
        
        
        updateFromEngine();
        
    }

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
    
    // cursor stuff:
    juce::MouseCursor zoomCursor;
    std::unique_ptr<juce::Drawable> drawable;
    juce::Image cursorImage;
    
    // follow transport conflics with the drag zoom control
    bool followTransportWasActive;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DragZoomControl)
};
