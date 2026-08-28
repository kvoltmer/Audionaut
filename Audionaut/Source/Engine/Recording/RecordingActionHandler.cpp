//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "RecordingActionHandler.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/PlayList/TransportLoop.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Channel/AudioChannel.h"

namespace audium {

RecordingActionHandler::RecordingActionHandler(std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
                       std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                       std::shared_ptr<TransportLoop> transportLoop_,
                       std::shared_ptr<PlayListScheduler> playListScheduler_) :
    audioTrackContainer(audioTrackContainer_),
    audioResourceContainer(audioResourceContainer_),
    transportLoop(transportLoop_),
    playListScheduler(playListScheduler_)
{
#if AUDIONAUT_HEADLESS
    transportLoop->onLoopEnteredFunction        = [this] { onLoopEntered(); };
    transportLoop->onLoopActionFunction         = [this] { onLoopAction(); };
    transportLoop->onPlayListItemUpdateFunction = [this] { onPlayListItemUpdate(); };
#endif
    
    playListScheduler->onRecordingStartedFunction = [this] { onRecordingStarted(); };
}

void RecordingActionHandler::onRecordingStarted()
{
    for (auto track : audioTrackContainer->getAudioTracks()) {
        
        if (track->isRecordEnabled() &&
            not track->isRecording()) {
            
            auto resourceGroup = track->createNewResourceGroup();
            std::vector<std::shared_ptr<AudioResource>> resources;
            
            for (auto chan : track->audioChannelContainer->objects) {
                if (chan->isRecordEnabled()) {
                    auto &arc = track->getAudioResourceContainer();
                    auto res = arc.addAudioResource({},
                                                    nullptr,
                                                    track,
                                                    resourceGroup,
                                                    chan->getChannelNumber(),
                                                    0);
                    resources.push_back(res);
                }
            }
            
            auto newItem = track->createDefaultPlayListItem(resources.front(),
                                             resourceGroup,
                                             playListScheduler->data.recordingStartPositionClocks,
                                             audium::clocks);
            newItem->needsLengthUpdate = true;
        }
    }
}

void RecordingActionHandler::onRecordingFinished()
{
    auto context = audium::seconds;
    audioResourceContainer->onRecordingFinished();

    auto loopRange = transportLoop->getLoopPositionRange(context);
    
    for (auto track : audioTrackContainer->getAudioTracks()) {
        for (auto item : track->getPlayListContainer()->getPlayListItems()) {
            if (not item->isRecording()) {
                if (item->getVoiceSources().size() == 0) {
                    item->createTransportSources();
                }
                item->needsLengthUpdate = false;
                
                auto recStart = item->getRecordingStartPosition(context);
                auto recLength = item->getRecordedLength(context);
                auto phase = transportLoop->getLoopPhaseForPosition(recStart,
                                                                    recLength,
                                                                    context);
                auto fraction = phase - std::floor(phase);
                auto regionData = item->getRegionData(context);
                
                if (item->isFirstPartInLoop) {
                    item->isFirstPartInLoop = false;

                    // first part is always the newest stuff
                    regionData.setEnd(recLength);
                    regionData.setStart(recLength - (fraction * loopRange.getLength()));
                    item->setRegionData(regionData, context);
                }
                else if (item->isSecondPartInLoop) {
                    item->isSecondPartInLoop = false;
                    
                    auto end = recLength - (fraction * loopRange.getLength());
                    auto len = (1.0 - fraction) * loopRange.getLength();
                    auto start = end - len;
                    regionData.setEnd(end);
                    regionData.setStart(start);
                    item->setRegionData(regionData, context);
                    
                    auto newPos = loopRange.getStart() + (fraction * loopRange.getLength());
                    item->setAbsolutePosition(newPos, context);
                }
                
//                auto pos = item->getAbsolutePosition(context);
//                std::cout << "pos " << pos << " start " << regionData.getStart() << " length " << regionData.getLength() << std::endl;
            }
        }
    }
}

void RecordingActionHandler::onLoopEntered()
{
    if (playListScheduler->isRecording()) {
        
        auto context = audium::clocks;
        
        for (auto track : audioTrackContainer->getAudioTracks()) {
            // clonePlayListItem() inserts into the vector getPlayListItems()
            // returns by reference - iterate a snapshot so the loop survives
            // the insert (and the fresh clones are not visited).
            auto items = track->getPlayListContainer()->getPlayListItems();
            for (auto item : items) {
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
                    juce::Range<double> theRange(regionLength, regionLength);
                    clone->setRegionData(theRange, context);
                    // the new item's position is where the loop range starts
                    clone->setAbsolutePosition(loopRange.getStart(), context);
                    track->getPlayListContainer()->sortByPosition();
                    clone->needsLengthUpdate = true;
                    
                }
            }
        }
    }
}

void RecordingActionHandler::onLoopAction()
{
    if (playListScheduler->isRecording()) {
        
        auto context = audium::clocks;
        auto loopRange = transportLoop->getLoopPositionRange(context);
        
        for (auto track : audioTrackContainer->getAudioTracks()) {
            // snapshot: the clone below inserts into the iterated vector
            auto items = track->getPlayListContainer()->getPlayListItems();
            for (auto item : items) {
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
                        clone->setRegionData({start, start}, context);
                        
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
                            regionData = regionData.movedToStartAt(regionData.getStart() + loopRange.getLength());
                            item->setRegionData(regionData, context);
                        }
                    }
                }
            }
        }
    }
}

void RecordingActionHandler::onPlayListItemUpdate()
{
    if (playListScheduler->isRecording()) {
        
        auto context = audium::clocks;
        
        for (auto track : audioTrackContainer->getAudioTracks()) {
            for (auto item : track->getPlayListContainer()->getPlayListItems()) {
                if (item->needsLengthUpdate) {
                    
                    // length is the recorded length less the region start
                    auto length = item->getRecordedLength(context) - item->getRegionData(context).getStart();
                    auto itemRange = item->getAbsolutePositionRange(context);
                    if (itemRange.getLength() < length) {
                        itemRange.setLength(length);
                    }
                    
                    if (transportLoop->isLoopActive() &&
                        transportLoop->isWithinLoop()) {
                        
                        auto loopRange = transportLoop->getLoopPositionRange(context);
                        
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
                            currLength = 0.0;
                        }
                        
                        if (item->isFirstPartInLoop) {
                            itemRange.setLength(currLength);
                        }
                        else if (item->isSecondPartInLoop) {
                            // std::cout << "currPos " << currPos << std::endl;
                            item->setAbsoluteStartPosition(currPos, context);
                            // continue to skip length update
                            continue;
                        }
                    }
                    
                    // !!! set the length of the play list item
                    auto currentLength = item->getRegion()->getRegionData(context).getLength();
                    if (not exactlyEqual (currentLength, itemRange.getLength())) {
                        item->getRegion()->setRegionLength(itemRange.getLength(), context);
                    }
                }
            }
        }
    }
}

} // namespace audium
