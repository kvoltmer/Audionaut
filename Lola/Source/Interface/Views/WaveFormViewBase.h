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
    WaveFormViewBase(juce::Component *parentComponent,
                     std::shared_ptr<AudiumEngine> audiumEngine,
                     std::shared_ptr<AudioResource> audioResource,
                     std::shared_ptr<ZoomHandler> zoomHandler,
                     juce::Colour colour,
                     std::shared_ptr<RegionSelector> regionSelector,
                     int channelNumber_) :
        parentComponent(parentComponent),
        audiumEngine(audiumEngine),
        audioResource(audioResource),
        zoomHandler(zoomHandler),
        colour(colour),
        regionSelector(regionSelector),
        channelNumber(channelNumber_)
    {
        // this component doesn't handle mouse events
        //setInterceptsMouseClicks(false, false);
        
        // use buffered image
        // setBufferedToImage(true);
        
        // create thumbnail
        auto thumbnailCache = audiumEngine->getAudioResourceContainer()->getAudioThumbnailCache().get();
        auto formatManager = audiumEngine->getAudioResourceContainer()->getAudioFormatManager().get();
        auto sourceSamplesPerThumbnailSample = 64;
        //auto sourceSamplesPerThumbnailSample = 256;
        //auto sourceSamplesPerThumbnailSample = 4096*4;
        audioThumbnail.reset(new audium::AudioThumbnail(sourceSamplesPerThumbnailSample, *formatManager, *thumbnailCache));
        audioThumbnail->setColour(colour);
        if (audioResource->audioFormatReader != nullptr) {
            
            // reuse shared_ptr if the file is memory mapped
            if (dynamic_cast<MemoryMappedAudioFormatReader*> (audioResource->audioFormatReader.get()) != nullptr) {
                auto hashCode = audioResource->getUrl().toString(true).hashCode64();
                audioThumbnail->setReader(audioResource->audioFormatReader, hashCode);
            }
            else {
            
                // create a new input source
                if (auto inputSource = std::make_unique<juce::URLInputSource>(audioResource->getUrl())) {
                    audioThumbnail->setSource(inputSource.release());
                }
            }
        }
        audioThumbnail->addChangeListener(this);
    }

    ~WaveFormViewBase() override
    {
        audioThumbnail->removeChangeListener(this);
    }
    
    void paint (juce::Graphics& g) override;
    
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    
    virtual double getRegionStart(audium::TimeContextType context) const = 0;
    
    virtual double getClipGain() const { return 1.0; }
    
    void paintBackground (juce::Graphics& g);

    void paintFileNameLabel (juce::Graphics& g);
    
    // returns the drawing area clipped with the visible area of the viewport
    // note: this is much faster than drawing the entire waveform
    juce::Rectangle<double> getClippedDrawingArea() const;
    
    virtual void updateUI(int theChannel) { channelNumber = theChannel; }
    
    void setParentComponent(juce::Component *comp) { parentComponent = comp; }
    
protected:
    juce::Component *parentComponent;
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioResource> audioResource;
    std::shared_ptr<ZoomHandler> zoomHandler;

    juce::Colour colour;
    std::shared_ptr<RegionSelector> regionSelector;
    
    std::unique_ptr<audium::AudioThumbnail> audioThumbnail;
    
    static constexpr float verticalZoomFactor = 1.f;
    
    int channelNumber = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveFormViewBase)
};
