//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListItem.h"

class ArrangementOverview  : public juce::Component
{
public:
    ArrangementOverview(std::shared_ptr<audium::AudiumEngine> audiumEngine,
                        bool arrangementMode) :
        audiumEngine(audiumEngine),
        arrangementMode(arrangementMode)
    {
        updateFromEngine();
    }

    ~ArrangementOverview() override = default;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
        g.setColour (juce::Colours::transparentBlack);
        g.drawRect (getLocalBounds(), 1);
    }
    

    void resized() override
    {
        updateFromEngine();
    }
    
    void updateFromEngine()
    {
        removeAllChildren();
        groupRectangles.clear();
        
        auto bounds = getLocalBounds().reduced(1, 1).toFloat();
        
        auto numGroups = audiumEngine->getAudioTrackContainer()->getNumItems();
        auto h = bounds.getHeight() / static_cast<float>(numGroups);
        auto y = bounds.getY();
        
        auto totalLength = audiumEngine->getPlayListScheduler()->getTotalLength(audium::seconds, true);
        jassert(totalLength > 0.0);
        int i = 0;
        for (auto track : audiumEngine->getAudioTrackContainer()->getAudioTracks())
        {
            for (auto item : track->getPositionableItems(arrangementMode))
            {
                groupRectangles.push_back(std::make_unique<juce::DrawableRectangle>());
                addAndMakeVisible(groupRectangles[i].get());
                groupRectangles[i]->setFill (track->getColour().withAlpha (0.375f));
                
                auto position = item->getAbsolutePositionRange(audium::seconds);
                auto relativePos = position.getStart() / totalLength;
                auto relativeLength = position.getLength() / totalLength;
                
                // calc start and width
                auto x = bounds.getX() + (bounds.getWidth() * relativePos);
                auto w = bounds.getWidth() * relativeLength;
                
                groupRectangles[i]->setRectangle(juce::Rectangle<float>(x, y, w, h));
                
                i++;
            }
            y += h;
        }
    }

private:
    std::vector<std::unique_ptr<juce::DrawableRectangle>> groupRectangles;
    
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    
    bool arrangementMode;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementOverview)
};
