/*
  ==============================================================================

    PlayListItemComponent.h
    Created: 28 Sep 2023 12:07:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Models/PlayListItemArrangementModel.h"
#include "Interface/Controls/SliderControl.h"

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
                          std::shared_ptr<PlayListItem> playListItem,
                          std::shared_ptr<ZoomHandler> zoomHandler,
                          std::shared_ptr<RegionSelector> regionSelector);
    ~PlayListItemComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void changeListenerCallback (ChangeBroadcaster* source) override;
    
    std::shared_ptr<PlayListItem> getPlayListItem() const { return playListItem; }
    
    DraggerControl* getDraggerControl() const;
    
    void updateUI();
    
    void mouseEnter (const MouseEvent& e) override;
    void mouseExit (const MouseEvent& e) override;
    
private:
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioTrack>     audioTrack;
    std::shared_ptr<PlayListItem>   playListItem;
    std::shared_ptr<RegionSelector> regionSelector;
        
    std::unique_ptr<audium::ListBox> playListItemListBox;
    std::unique_ptr<PlayListItemArrangementModel> playListItemArrangementModel;
    std::unique_ptr<SliderControl> volumeSlider;
        
    std::unique_ptr<FadeInOutControl> fadeInControl;
    std::unique_ptr<FadeInOutControl> fadeOutControl;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListItemComponent)
};
