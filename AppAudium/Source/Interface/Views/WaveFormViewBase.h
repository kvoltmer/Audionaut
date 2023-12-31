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
#include "Engine/AudioResource.h"
#include "Engine/AudioResourceContainer.h"
#include "Interface/ColourIds.h"
#include "Engine/AudioRegion.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Factory/AudioResourceFactory.h"
#include "Interface/Widgets/audium_AudioThumbnail.h"

//==============================================================================
/*
*/
class WaveFormViewBase : public juce::Component, public juce::ChangeListener
{
public:
    WaveFormViewBase(std::shared_ptr<AudiumEngine> audiumEngine,
                     std::shared_ptr<AudioResource> audioResource,
                     std::shared_ptr<ZoomHandler> zoomHandler,
                     std::shared_ptr<AudioRegion> audioRegion,
                     juce::Colour colour,
                     std::shared_ptr<RegionSelector> regionSelector,
                     int rowNumber) :
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
        audioThumbnail.reset(new audium::AudioThumbnail(4096*4, *formatManager, *thumbnailCache));
        audioThumbnail->setColour(colour);
        if (auto inputSource = AudioResourceFactory::makeAudioInputSource(audioResource->getUrl()))
        {
            audioThumbnail->setSource(inputSource.release());
        }
        audioThumbnail->addChangeListener(this);
    }

    ~WaveFormViewBase() override
    {
        audioThumbnail->removeChangeListener(this);
    }
    
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        // this method is called by the thumbnail when it has changed, so we should repaint it..
        repaint();
    }
    
    void paintBackground (juce::Graphics& g)
    {
        // paint a vertical gradient as background:
        
        // bottom colour = transparent version of the group colour
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
    
    /// TODO: row number might have changed after delete
    void setRowNumber(int rowNumber) {}

protected:
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioResource> audioResource;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<AudioRegion> audioRegion;
    juce::Colour colour;
    std::shared_ptr<RegionSelector> regionSelector;
    
    std::unique_ptr<audium::AudioThumbnail> audioThumbnail;
    
    static constexpr float verticalZoomFactor = 1.f;
    
    int rowNumber;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveFormViewBase)
};
