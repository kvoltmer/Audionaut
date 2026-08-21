//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Controls/RegionSelector.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Interface/ColourIds.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Factory/AudioResourceFactory.h"
#include "Interface/Widgets/audium_AudioThumbnail.h"
#include "Engine/Resource/ChannelMapping.h"

//==============================================================================
/*
*/
class WaveFormViewBase : public juce::Component, public juce::ChangeListener
{
public:
    WaveFormViewBase(juce::Component *parentComponent_,
                     std::shared_ptr<audium::AudiumEngine> audiumEngine_,
                     std::shared_ptr<audium::AudioResource> audioResource_,
                     std::shared_ptr<ZoomHandler> zoomHandler_,
                     juce::Colour colour_,
                     std::shared_ptr<RegionSelector> regionSelector_,
                     int channelNumber_,
                     std::shared_ptr<audium::PlayListItem> playListItem_) :
        parentComponent(parentComponent_),
        audiumEngine(audiumEngine_),
        audioResource(audioResource_),
        zoomHandler(zoomHandler_),
        colour(colour_),
        regionSelector(regionSelector_),
        channelNumber(channelNumber_),
        playListItem(playListItem_)
    {
        // this component doesn't handle mouse events
        //setInterceptsMouseClicks(false, false);
        
        // use buffered image
        // setBufferedToImage(true);
        
        createThumbnailCache();
    }

    ~WaveFormViewBase() override
    {
        if (audioThumbnail != nullptr) {
            audioThumbnail->removeChangeListener(this);
        }
    }
    
    void paint (juce::Graphics& g) override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    /** Builds a cache-backed thumbnail for a file-based resource; returns
        nullptr for resources without a reader (live recordings). The heavy
        low-res data is shared through the engine's AudioThumbnailCache. */
    static std::shared_ptr<audium::AudioThumbnail> createThumbnailForResource(
        const std::shared_ptr<audium::AudiumEngine>& audiumEngine,
        const std::shared_ptr<audium::AudioResource>& audioResource);
    
    virtual double getRegionStart(audium::TimeContextType context) const = 0;
    
    virtual double getClipGain() const { return 1.0; }
    
    void paintBackground (juce::Graphics& g);

    void paintFileNameLabel (juce::Graphics& g);
    
    // returns the drawing area clipped with the visible area of the viewport
    // note: this is much faster than drawing the entire waveform
    juce::Rectangle<double> getClippedDrawingArea() const;
    
    virtual void updateUI(std::shared_ptr<audium::AudioResource> audioResource,
                          int theChannel) = 0;
    
    void setParentComponent(juce::Component *comp) { parentComponent = comp; }
    
protected:
    juce::Component *parentComponent;
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<audium::AudioResource> audioResource;
    std::shared_ptr<audium::PlayListItem> playListItem = nullptr;
    std::shared_ptr<ZoomHandler> zoomHandler;

    juce::Colour colour;
    std::shared_ptr<RegionSelector> regionSelector;
    
    std::shared_ptr<audium::AudioThumbnail> audioThumbnail;
    
    static constexpr float verticalZoomFactor = 1.f;
    
    int channelNumber = 0;
    
    void createThumbnailCache();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveFormViewBase)
};
