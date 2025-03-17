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
#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Models/PlayListItemArrangementModel.h"

class AudioTrack;
class PlayListItem;
class ZoomHandler;
class RegionSelector;
class AudiumEngine;
class DraggerControl;
class FadeInOutControl;

//==============================================================================
/*
Display a PlayListItem within a AudioTrack
*/
class PlayListItemComponent  : public juce::Component, public juce::ChangeListener
{
public:
    PlayListItemComponent(std::shared_ptr<AudiumEngine> audiumEngine,
                          std::shared_ptr<AudioTrack> audioTrack,
                          std::shared_ptr<PlayListContainer> playListContainer,
                          std::shared_ptr<ZoomHandler> zoomHandler,
                          std::shared_ptr<RegionSelector> regionSelector);
    ~PlayListItemComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void changeListenerCallback (ChangeBroadcaster* source) override;
    
    std::shared_ptr<PlayListItem> getPlayListItem() const { return playListItem; }
    void setPlayListItem(std::shared_ptr<PlayListItem> item);
    
    DraggerControl* getDraggerControl() const;
    
    void updateUI(std::shared_ptr<PlayListItem> playListItem);
    
    void mouseEnter (const MouseEvent& e) override;
    void mouseExit (const MouseEvent& e) override;
    
private:
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioTrack>     audioTrack;
    std::shared_ptr<PlayListItem>   playListItem;
    std::shared_ptr<RegionSelector> regionSelector;
        
    std::unique_ptr<audium::ListBox> playListItemListBox;
    std::unique_ptr<PlayListItemArrangementModel> playListItemArrangementModel;
    
        
    std::unique_ptr<FadeInOutControl> fadeInControl;
    std::unique_ptr<FadeInOutControl> fadeOutControl;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListItemComponent)
};
