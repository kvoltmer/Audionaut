//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>
#include "FadeInOutView.h"

using namespace juce;

void FadeInOutView::paint (juce::Graphics& g)
{
    jassert(playListItem);
    auto fFadeInWidth   = playListItem->getFadeIn() * getWidth();
    auto iFadeInWidth   = static_cast<int>(fFadeInWidth);
    auto fFadeOutWidth  = playListItem->getFadeOut() * getWidth();
    auto iFadeOutWidth  = static_cast<int>(fFadeOutWidth);
    auto fHeight        = static_cast<float>(getHeight());
    auto fWidth         = static_cast<float>(getWidth());
    auto yOffset        = 1.f;
    juce::Path thePath;
    
    if (iFadeInWidth > 0) {
        thePath.startNewSubPath (Point<float> (0.f, fHeight));
        auto stride = iFadeInWidth > 10 ? 4 : 1;
        for (auto w = 0; w < iFadeInWidth; w += stride) {
            auto square = powf(w / fFadeInWidth, 0.5f); // square root
            auto y      = fHeight - (square * fHeight);
            
            thePath.lineTo(static_cast<float>(w), y);
        }
        thePath.lineTo(fFadeInWidth, yOffset);
        if (iFadeOutWidth == 0) {
            thePath.lineTo(fWidth, yOffset);
            thePath.lineTo(fWidth, fHeight);
        }
    }
    
    if (iFadeOutWidth > 0) {
        
        if (iFadeInWidth == 0) {
            thePath.startNewSubPath (Point<float> (0.f, fHeight));
            thePath.lineTo(0.f, yOffset);
        }
        
        auto stride = iFadeOutWidth > 10 ? 4 : 1;
        for (auto w = 0; w < iFadeOutWidth; w += stride) {
            auto square = powf(1.f - (w / fFadeOutWidth), 0.5f); // square root
            auto y      = fHeight - (square * fHeight);
            
            thePath.lineTo(fWidth - fFadeOutWidth + static_cast<float>(w), y);
        }
        thePath.lineTo(fWidth, fHeight);
    }
    
    
    if (iFadeInWidth > 0 ||
        iFadeOutWidth > 0) {
    
        juce::Path roundedPath = thePath.createPathWithRoundedCorners(4);
        
        g.setColour(Colour(Colours::white).withAlpha(0.1f));
        g.fillPath(roundedPath);
        
        g.setColour(Colour(Colours::white).withAlpha(0.5f));
        g.strokePath(roundedPath, PathStrokeType (2.f));
        
    }
    
}

void FadeInOutView::resized()
{
}

void FadeInOutView::setPlayListItem(std::shared_ptr<audium::PlayListItem> item)
{
    playListItem = item;
}
