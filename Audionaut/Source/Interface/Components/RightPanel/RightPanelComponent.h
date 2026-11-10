//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Interface/UIContext.h"
#include "Interface/Controls/CollapsiblePanel.h"
#include "Interface/Controls/PanelHeaderBar.h"
#include "Engine/AudiumEngine.h"

class PlayListContainerComponent;
class RegionComponent;
class RegionContainerComponent;

/**
 * The panel right of the arrangement: the playlists on top, the region list
 * under them. The region list opens and closes at
 * its header bar as the mixer does under the arrangement; whether it is open
 * and how tall the user dragged it are app preferences.
 */
class RightPanelComponent  : public juce::Component
{
public:
    RightPanelComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine);
    ~RightPanelComponent() override;
    
    void paint (juce::Graphics&) override;
    void resized() override;
    void updateUI(UIContext context = ContentContext);

private:
    static constexpr int minPlayListsHeight = 100;    ///< what the region list leaves the playlists above it
    static constexpr int minRegionListHeight = 100;
    static constexpr int defaultRegionListHeight = 300;

    void readLayoutPreferences();
    void writeLayoutPreferences();

    std::shared_ptr<audium::AudiumEngine> audiumEngine;

    std::unique_ptr<RegionComponent> regionComponent;
    std::unique_ptr<PlayListContainerComponent> playListContainerComponent;
    PanelHeaderBar regionListHeader { "Regions", TRANS ("Show or hide the region list") };
    CollapsiblePanel regionList { minRegionListHeight, defaultRegionListHeight };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RightPanelComponent)
};
