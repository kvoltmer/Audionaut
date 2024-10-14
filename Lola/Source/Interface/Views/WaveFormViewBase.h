/*
  ==============================================================================

    WaveFormViewBase.h
    Created: 27 Nov 2023 4:15:50pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

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

//==============================================================================
/*
*/
class WaveFormViewBase : public juce::Component, public juce::ChangeListener
{
public:
    WaveFormViewBase(const juce::Component &parentComponent,
                     std::shared_ptr<AudiumEngine> audiumEngine,
                     std::shared_ptr<AudioResource> audioResource,
                     std::shared_ptr<ZoomHandler> zoomHandler,
                     std::shared_ptr<AudioRegion> audioRegion,
                     juce::Colour colour,
                     std::shared_ptr<RegionSelector> regionSelector,
                     int rowNumber) :
        parentComponent(parentComponent),
        audiumEngine(audiumEngine),
        audioResource(audioResource),
        zoomHandler(zoomHandler),
        audioRegion(audioRegion),
        colour(colour),
        regionSelector(regionSelector),
        rowNumber(rowNumber)
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
        if (auto inputSource = AudioResourceFactory::makeAudioInputSource(audioResource->getUrl()))
        {
            //audioThumbnail->setReader(audioResource->getAudioFormatReader().get(), inputSource->hashCode());
            audioThumbnail->setSource(inputSource.release());
        }
        audioThumbnail->addChangeListener(this);
    }

    ~WaveFormViewBase() override
    {
        audioThumbnail->removeChangeListener(this);
    }
    
    void paint (juce::Graphics& g) override
    {
        
        paintBackground(g);
        
        jassert(audioResource != nullptr);
        
        if (audioThumbnail->getTotalLength() > 0.0)
        {
            // the waveform colour
            g.setColour (colour);
                    
            const auto start        = getRegionStart(audium::seconds);
            const auto thumbArea    = getClippedDrawingArea();
            const auto startSeconds = zoomHandler->xToSeconds(thumbArea.getX()) + start;
            const auto endSeconds   = startSeconds + zoomHandler->xToSeconds(thumbArea.getWidth());
            
            const auto channel = rowNumber - audioResource->getChannelPosition();
            if (channel >= 0 && channel < audioResource->getNumChannels())
            {
                audioThumbnail->drawChannel(g, thumbArea.toNearestInt(), startSeconds, endSeconds, channel, verticalZoomFactor);
            }
            else
            {
                std::cout << "error WaveFormViewBase channel mapping." << std::endl;
            }
        }
    }
    
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        // this method is called by the thumbnail when it has changed, so we should repaint it..
        repaint();
    }
    
    //
    virtual double getRegionStart(audium::TimeContextType context) const = 0;
    
    void paintBackground (juce::Graphics& g)
    {
        // paint a vertical gradient as background:
        
        // bottom colour = transparent version of the track colour
        const auto bottomColour = colour.withAlpha(0.375f);
        
        // top colour = complementary colour of bottom
        const auto topColour = audium::getComplementaryColour(bottomColour);
        
        auto colourGradient = juce::ColourGradient::vertical(topColour, getLocalBounds().getTopLeft().getY(),
                                                             bottomColour, getLocalBounds().getBottomLeft().getY());
        g.setGradientFill(colourGradient);
        g.fillAll();
    }

    void paintFileNameLabel (juce::Graphics& g)
    {
        /// draw filename label
        /// offset is x = 5, y = 5
        /// background is expanded by 2 pixels
        
        g.setFont (12.0f);
        
        juce::Rectangle<int> bonds(5,
                             5,
                             g.getCurrentFont().getStringWidth(audioResource->getFileNameWithoutExtension()),
                             g.getCurrentFont().getHeight());
        
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillRoundedRectangle (bonds.expanded(2, 2).toFloat(), 3.0f);
        
        g.setColour (findColour(audium::defaultTextColourId));
        g.drawFittedText (audioResource->getFileNameWithoutExtension(), bonds, juce::Justification::topLeft, 1);
    }
    
    // returns the drawing area clipped with the visible area of the viewport
    // note: this is much faster than drawing the entire waveform
    juce::Rectangle<double> getClippedDrawingArea() const
    {
        // the local bounds
        auto thumbArea = getLocalBounds().toDouble();
    
        // the visible range of the viewport
        const auto visibleRange = zoomHandler->getVisibleRange();
        
        // clip to the area of its parent (owner)
        const auto parentOffset = static_cast<double>(parentComponent.getBounds().getX());
        //std::cout << parentComponent.getName().toStdString() << " offset: " << parentOffset << std::endl;
        const auto scrollOffset = zoomHandler->getVisibleRange().getStart();
        const auto startX = std::max(scrollOffset - parentOffset, 0.0);
        const auto lengthX = std::min(visibleRange.getLength(), static_cast<double>(thumbArea.getWidth()) - startX);
    
        thumbArea.setX(startX);
        thumbArea.setWidth(lengthX);
        //std::cout << thumbArea.getX() << " " << thumbArea.getWidth() << std::endl;
        return thumbArea;
    }
    
    void setRowNumber(int theRowNumber) { rowNumber = theRowNumber; }

protected:
    const juce::Component &parentComponent;
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioResource> audioResource;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<AudioRegion> audioRegion;
    juce::Colour colour;
    std::shared_ptr<RegionSelector> regionSelector;
    
    std::unique_ptr<audium::AudioThumbnail> audioThumbnail;
    
    static constexpr float verticalZoomFactor = 1.f;
    
    int rowNumber = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveFormViewBase)
};
