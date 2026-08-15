//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <algorithm>
#include <cmath>

#include "SnapToGridHandler.h"
#include "Interface/Handlers/ZoomHandler.h"

void SnapToGridHandler::publishRange(juce::Range<double> clocks, bool excludeSelectedItems_)
{
    excludeSelectedItems = excludeSelectedItems_;
    
    // std::cout << "publishRange " << clocks.getStart() << " " << clocks.getEnd() << std::endl;
    clockRange = clocks;
    sendChangeMessage();
}

void SnapToGridHandler::clearRange()
{
    excludeSelectedItems = false;
    clockRange = juce::Range<double>();
    sendChangeMessage();
}

int roundSecondsToGrid(int x)
{
    if (x < 1)
    {
        return 1;
    }
    else if (x < 5)
    {
        return 5;
    }
    else if (x < 15)
    {
        return 15;
    }
    else
    {
        return x + 30 - x % 30;
    }
}

int roundBarsToGrid(int x)
{
    if (x < 1)
    {
        return 1;
    }
    else if (x < 4)
    {
        return 4;
    }
    else if (x < 16)
    {
        return 16;
    }
    else
    {
        return x + 32 - x % 32;
    }
}

int roundBeatsToGrid(int x)
{
    if (x < 1)
    {
        return 1;
    }
    else if (x < 4)
    {
        return 4;
    }
    else if (x < 16)
    {
        return 16;
    }
    else
    {
        return x + 32 - x % 32;
    }
}

const std::vector<SnapToGridHandler::Segment> SnapToGridHandler::getGridInSegments(const double widthInPixels,
                                                                                   const double lengthInSegmentType,
                                                                                   SnapToGridHandler::SegmentType type,
                                                                                   bool includePlayListItems,
                                                                                   double startInSegmentType)
{
    std::vector<SnapToGridHandler::Segment> segmentList;

    if (widthInPixels > 0.0 &&
        lengthInSegmentType > 0.0) {

        auto pixelsPerSegmentType = widthInPixels / lengthInSegmentType;

        auto grid = 0.0;

        switch (type) {
            case seconds:
                grid = roundSecondsToGrid(static_cast<int>(100.0 / pixelsPerSegmentType));
                break;
            case beats:
                grid = roundBeatsToGrid(static_cast<int>(25.0 / pixelsPerSegmentType));
                break;
            case bars:
                grid = roundBarsToGrid(static_cast<int>(50.0 / pixelsPerSegmentType));
                break;
            default:
                break;
        }

        // the last grid line at or before the requested start, so a segment
        // whose label reaches into the range is included as well
        const auto firstSegment = std::floor(std::max(0.0, startInSegmentType) / grid);

        auto numSegments = (lengthInSegmentType / grid) + 2;
        for (auto i = 0; i < numSegments; i++) {
            SnapToGridHandler::Segment seg;
            seg.position = (firstSegment + i) * grid;
            seg.grid = grid;
            seg.type = type;
            segmentList.push_back(seg);
        }
    }
    
    if (includePlayListItems) {
        auto items = getPlayListItemsInSegments(type);
        for (auto item : items) {
            segmentList.push_back(item);
        }
        std::sort(segmentList.begin(), segmentList.end(),
                  [](const SnapToGridHandler::Segment i1, const SnapToGridHandler::Segment i2)
                  {
            return (i1.position < i2.position);
        });
        
        for (auto s : segmentList) {
            // check for duplicates
        }
        
    }
    

    
    return segmentList;
}

const std::vector<SnapToGridHandler::Segment> SnapToGridHandler::getPlayListItemsInSegments(SnapToGridHandler::SegmentType type) const
{
    jassert(type == beats);
    std::vector<SnapToGridHandler::Segment> segmentList;

    auto playListItems = playListScheduler->getPlayListItems(excludeSelectedItems);
    for (auto item : playListItems) {
        
        auto posRange = item->getAbsolutePositionRange(audium::clocks);
        
        SnapToGridHandler::Segment segStart;
        segStart.position = audium::TempoProvider::clocksToBeats(posRange.getStart());
        segStart.grid = 0.0;
        segStart.type = type;
        segStart.isPlayListItem = true;
        segmentList.push_back(segStart);
        
        SnapToGridHandler::Segment segEnd;
        segEnd.position = audium::TempoProvider::clocksToBeats(posRange.getEnd());
        segEnd.grid = 0.0;
        segEnd.type = type;
        segEnd.isPlayListItem = true;
        segmentList.push_back(segEnd);
    }
    
//    std::sort( vec.begin(), vec.end() );
//    vec.erase( std::unique( vec.begin(), vec.end() ), vec.end() );
    
    return segmentList;
}

bool SnapToGridHandler::snapToGrid(const ZoomHandler& zoomHandler,
                                   double &clocks)
{
    if (!juce::ModifierKeys::currentModifiers.isShiftDown()) {
        
        auto beats          = audium::TempoProvider::clocksToBeats(clocks);
        auto tolerance      = zoomHandler.xToBeats(10.0);
        
        auto lengthInBeats = zoomHandler.xToBeats(zoomHandler.getContentWidth());
        auto segmentList = getGridInSegments(zoomHandler.getContentWidth(),
                                             lengthInBeats,
                                             SnapToGridHandler::beats,
                                             true);

        for (auto seg : segmentList) {
            if (std::abs(seg.position - std::max(beats, 0.0)) < tolerance) {
                //std::cout << "snap to: " << seg.position << std::endl;
                clocks = audium::TempoProvider::beatsToClocks(seg.position);
                return true;
            }
        }
    }
    
    return false;
}

bool SnapToGridHandler::snapToGrid(const ZoomHandler& zoomHandler,
                                   juce::Range<double> &clocks)
{
    double start = clocks.getStart();
    double end = clocks.getEnd();
    
    bool snapStart = snapToGrid(zoomHandler, start);
    bool snapEnd = snapToGrid(zoomHandler, end);
        
    if (snapStart && snapEnd) {
        clocks = juce::Range<double>(start, end);
    }
    else if (snapStart && !snapEnd) {
        clocks = clocks.movedToStartAt(start);
    }
    else if (!snapStart && snapEnd) {
        clocks = clocks.movedToEndAt(end);
    }
 
    return (snapStart || snapEnd);
}
