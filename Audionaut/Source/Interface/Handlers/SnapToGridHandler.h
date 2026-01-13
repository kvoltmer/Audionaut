//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/Selection/SelectionManager.h"
#include "Engine/PlayList/PlayListScheduler.h"

class ZoomHandler;

class SnapToGridHandler : public juce::ChangeBroadcaster {
    
public:
    SnapToGridHandler(std::shared_ptr<audium::PlayListScheduler> playListScheduler_) :
        playListScheduler(playListScheduler_)
    {
    }
    
    void publishRange(juce::Range<double> clocks, bool excludeSelectedItems = false);
    
    void clearRange();
    
    juce::Range<double> getRange() const { return clockRange; }

    enum SegmentType {
        seconds = 0,
        bars = 1,
        beats = 2
    };
    
    struct Segment {
        SegmentType type = beats;
        double position = 0.0;
        double grid = 0.0;
        bool isPlayListItem = false;
    };
    
    const std::vector<Segment> getGridInSegments(const double widthInPixels,
                                                 const double lengthInSegmentType,
                                                 SnapToGridHandler::SegmentType type,
                                                 bool includePlayListItems = false);
    
    const std::vector<Segment> getPlayListItemsInSegments(SnapToGridHandler::SegmentType type) const;
    
    bool snapToGrid(const ZoomHandler& zoomHandler, double &clocks);
    bool snapToGrid(const ZoomHandler& zoomHandler, juce::Range<double> &clocks);
    
private:
    
    juce::Range<double> clockRange;
    bool excludeSelectedItems = false;
    
    std::shared_ptr<audium::PlayListScheduler> playListScheduler;
};
