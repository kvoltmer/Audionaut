//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "RecordingActionHandler.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/PlayList/TransportLoop.h"

namespace audium {

void RecordingActionHandler::onRecordingFinished()
{
    audioResourceContainer->onRecordingFinished();

    for (auto track : audioTrackContainer->getAudioTracks()) {
        for (auto playListItem : track->getPlayListContainer()->getPlayListItems()) {
            if (not playListItem->isRecording()) {
                if (playListItem->getTransportSources().size() == 0) {
                    playListItem->createTransportSources();
                }
                playListItem->needsLengthUpdate = false;
            }
        }
    }
}

void RecordingActionHandler::onLoopEntered()
{
    auto context = audium::clocks;
    
    for (auto track : audioTrackContainer->getAudioTracks()) {
        for (auto item : track->getPlayListContainer()->getPlayListItems()) {
            if (item->isRecording()) {
                
                // split play list items in 2 parts
                
                // part BEFORE loop start
                auto loopRange = transportLoop->getLoopPositionRange(context);
                item->setAbsoluteEndPosition(loopRange.getStart(), context);
                item->needsLengthUpdate = false;
                
                // part AFTER loop start (new item)
                auto clone = track->getPlayListContainer()->clonePlayListItem(item);
                
                // new region start is the length of the old region
                auto regionLength = item->getAbsolutePositionRange(context).getLength();
                juce::Range<double> theRange(regionLength, regionLength + 0.1);
                clone->setRegionData(theRange, context);
                // the new item's position is where the loop range starts
                clone->setAbsolutePosition(loopRange.getStart(), context);
                track->getPlayListContainer()->sortByPosition();
                clone->needsLengthUpdate = true;
                
            }
        }
    }
}

void RecordingActionHandler::onLoopAction()
{
    auto context = audium::clocks;
    auto loopRange = transportLoop->getLoopPositionRange(context);
        
    for (auto track : audioTrackContainer->getAudioTracks()) {
        for (auto item : track->getPlayListContainer()->getPlayListItems()) {
            if (item != nullptr &&
                item->isRecording() &&
                item->needsLengthUpdate) {
                
                // calc loop count based on recording start pos and recorded length (duration)
                auto loop = audioTrackContainer->getTransportLoop();
                auto phase = loop->getLoopPhaseForPosition(item->getRecordingStartPosition(context),
                                                           item->getRecordedLength(context),
                                                           context);
                // timing is a bit off so we need to add 0.1 to the phase (0.9947 + 0.1 = 1)
                auto count = static_cast<int>(phase + 0.1);
                
                // if we hit the loop the first time, we split the current item into 2
                if (count == 1) {
                    
                    // clone is FIRST part in the loop
                    auto clone = track->getPlayListContainer()->clonePlayListItem(item);
                    
                    // region start -> old region's start + length
                    auto start = item->getRegionData(context).getStart() + item->getRegionData(context).getLength();
                    clone->setRegionData({start, start + 0.1}, context);
                    
                    // position at loop start pos
                    clone->setAbsolutePosition(loopRange.getStart(), context);
                    clone->needsLengthUpdate = true;
                    clone->isFirstPartInLoop = true;
                    
                    // current item marked as SECOND part in the loop
                    jassert(item->needsLengthUpdate);
                    item->isSecondPartInLoop = true;
                    
                    track->getPlayListContainer()->sortByPosition();
                }
                else if (count > 1) {
                    // for all other loop events we simply move the region start by the loop length
                    if (item->isFirstPartInLoop ||
                        item->isSecondPartInLoop) {
                        auto regionData = item->getRegionData(context);
                        // TODO: better store the absolute position of the recording start and calc the accurate region start (% loop length)
                        regionData = regionData.movedToStartAt(regionData.getStart() + loopRange.getLength());
                        item->setRegionData(regionData, context);
                    }
                }
            }
        }
    }
}

void RecordingActionHandler::onTimerUpdate()
{
    bool needToUpdate = false;
    auto context = audium::clocks;
    auto loopRange = transportLoop->getLoopPositionRange(context);
    
    for (auto track : audioTrackContainer->getAudioTracks()) {
        for (auto item : track->getPlayListContainer()->getPlayListItems()) {
            if (item->needsLengthUpdate) {
                
                // length is the recorded length less the region start
                auto length = item->getRecordedLength(context) - item->getRegionData(context).getStart();
                auto itemRange = item->getAbsolutePositionRange(context);
                if (itemRange.getLength() < length) {
                    itemRange.setLength(length);
                }
                                
                if (transportLoop->isLoopActive()) {
                    
                    // length must not exceed loop end
                    if (itemRange.intersects(loopRange)) {
                        if (itemRange.getEnd() > loopRange.getEnd()) {
                            itemRange.setEnd(loopRange.getEnd());
                        }
                    }
                    
                    auto currPos = transportLoop->getCurrentPosition(context);
                    // std::cout << "loop " << loopRange.getStart() << " " << currPos << " " << loopRange.getEnd() << std::endl;
                    
                    auto currLength = currPos - loopRange.getStart();
                    if (currLength < 0.0) {
                        currLength = 0.1;
                    }
                    
                    if (item->isFirstPartInLoop) {
                        itemRange.setLength(currLength);
                    }
                    else if (item->isSecondPartInLoop) {
                        item->setAbsoluteStartPosition(currPos, context);  
                    }
                }
                
                if (not item->isSecondPartInLoop) {
                    item->getRegion()->setRegionLength(itemRange.getLength(), context);
                }
                needToUpdate = true;
            }
        }
    }
    
    if (needToUpdate)
        audioTrackContainer->sendActionMessage(audium::updateArrangementAction);
}

} // namespace audium
