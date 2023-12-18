/*
  ==============================================================================

    PlayListItemComponent.h
    Created: 28 Sep 2023 12:07:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class AudioGroup;
class AudioRegion;
class PlayListItem;
class ZoomHandler;
class AudioRegionView;
class RegionSelector;
class AudiumEngine;

//==============================================================================
/*
Display all AudioRegionViews within a AudioGroup
*/
class PlayListItemComponent  : public juce::Component
{
public:
    PlayListItemComponent(std::shared_ptr<AudiumEngine> audiumEngine,
                          std::shared_ptr<AudioGroup> audioGroup,
                          std::shared_ptr<PlayListItem> playListItem,
                          std::shared_ptr<ZoomHandler> zoomHandler,
                          std::shared_ptr<RegionSelector> regionSelector);
    ~PlayListItemComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::shared_ptr<PlayListItem> getPlayListItem() const { return playListItem; }
    
private:
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioGroup>     audioGroup;
    std::shared_ptr<PlayListItem>   playListItem;
    std::shared_ptr<RegionSelector> regionSelector;
    
    std::vector<std::shared_ptr<AudioRegionView>> children;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListItemComponent)
};
