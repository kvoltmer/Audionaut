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
                
                groupRectangles[i]->setRectangle(Rectangle<float>(x, y, w, h));
                
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
