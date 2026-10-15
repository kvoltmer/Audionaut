//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "TransportLoop.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/Provider/TempoProvider.h"

namespace audium {

void TransportLoop::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    externalSampleRate = sampleRate;
}

void TransportLoop::setLoopPositionRange(std::shared_ptr<AudioTrackContainer> audioTrackContainer,
                                         juce::Range<double> newRange,
                                         audium::TimeContextType context)
{
    if (newRange.getStart() >= 0.0) {
        
        auto minLength = loopData.minimumLoopLengthClocks;
        if (context == audium::seconds)
            minLength = tempoProvider->clocksToSeconds(loopData.minimumLoopLengthClocks);
        
        if (newRange.getLength() >= minLength) {
            
            // undo
            std::unique_ptr<UndoableContainerAction> action = nullptr;
            if (audioTrackContainer != nullptr)
                action = std::make_unique<audium::UndoableContainerAction>(*audioTrackContainer.get(), false);
            
            if (context == audium::seconds)
                newRange = tempoProvider->secondsToClocks(newRange);

            // loopCount is rescaled to the new length by the audio thread
            // when it takes the range over (pullLoopData), so both change
            // in the same block
            loopData.loopStartPositionClocks = newRange.getStart();
            loopData.loopEndPositionClocks = newRange.getEnd();
            publishLoopData();


            // undo
            if (action != nullptr &&
                undoManager != nullptr) {
                action->storeNewState();
                undoManager->perform(action.release(), "Change Loop");
                undoManager->beginNewTransaction();
            }
        }
    }
    else {
        std::cout << "setLoopPositionRange invalid range: " << newRange.getStart() << " " << newRange.getEnd() << std::endl;
    }
    
}

juce::Range<double> TransportLoop::getLoopPositionRange(audium::TimeContextType context) const
{
    return loopRangeOf(loopData, context);
}

juce::Range<double> TransportLoop::getProcessedLoopPositionRange(audium::TimeContextType context) const
{
    return loopRangeOf(processedLoopData, context);
}

juce::Range<double> TransportLoop::loopRangeOf(const LoopData& data, audium::TimeContextType context) const
{
    juce::Range<double> range(data.loopStartPositionClocks,
                              data.loopEndPositionClocks);
    if (context == audium::clocks) {
        return range;
    }
    else if (context == audium::seconds) {
        return tempoProvider->clocksToSeconds(range);
    }
    
    return juce::Range<double>(0.0, 0.0);
}

bool TransportLoop::isLoopActive() const
{
    return loopData.loopActive;
}

void TransportLoop::setLoopActive(bool bActive)
{
    loopData.loopActive = bActive;
    publishLoopData();
}

void TransportLoop::setLoopData(const LoopData& newData)
{
    loopData = newData;
    publishLoopData();
}

void TransportLoop::publishLoopData()
{
    // one-element snapshot; the copy happens on this (the message) thread
    publishedLoopData.getProducerObjects().assign(1, loopData);
    publishedLoopData.commit();
}

void TransportLoop::pullLoopData()
{
    if (! publishedLoopData.pull())
        return;

    const auto& snapshot = publishedLoopData.getConsumerObjects();
    if (snapshot.empty())
        return;

    const auto& next = snapshot.front();

    // keep the time already looped when the length changes, so the
    // position (which has loopCount lengths subtracted) does not jump
    if (processedLoopData.loopActive && loopCount > 0) {
        const auto oldLength = processedLoopData.loopEndPositionClocks - processedLoopData.loopStartPositionClocks;
        const auto newLength = next.loopEndPositionClocks - next.loopStartPositionClocks;
        if (newLength > 0.0 && ! juce::exactlyEqual(newLength, oldLength))
            loopCount = static_cast<int>(loopCount * oldLength / newLength);
    }

    processedLoopData = next;
}

const TransportLoop::LoopResult TransportLoop::processLoop(double thePosition,
                                                           int numSamples,
                                                           audium::TimeContextType context)
{
    TransportLoop::LoopResult result;
    result.context = context;

    // this block's loop data: taken over whole, read from nowhere else
    pullLoopData();
    auto loopRange = loopRangeOf(processedLoopData, context);

    jassert(externalSampleRate > 0.0);
    auto thisBuffer = static_cast<double>(numSamples) / externalSampleRate;
    
    if (context == audium::clocks)
        thisBuffer = tempoProvider->secondsToClocks(thisBuffer);
    
    // subtract previous loops
    thePosition -= (static_cast<double>(loopCount) * loopRange.getLength());
    // jassert(thePosition >= 0.0);

    if (processedLoopData.loopActive) {
        if (withinLoop &&
            thePosition + thisBuffer > loopRange.getEnd()) {
            
            auto diff = thePosition + thisBuffer - loopRange.getEnd();
            jassert(diff >= 0.0);
            
            result.timeUntilLoop = thisBuffer - diff;
            if (result.timeUntilLoop < 0.0)
                result.timeUntilLoop = 0.0;
            
            // calc samples until loop
            auto secondsUntilLoop = result.timeUntilLoop;
            if (context == audium::clocks)
                secondsUntilLoop = tempoProvider->clocksToSeconds(result.timeUntilLoop);
            result.numSamplesUntilLoop = static_cast<int>(std::round(secondsUntilLoop * externalSampleRate));
            jassert(result.numSamplesUntilLoop >= 0);
            
            if (result.numSamplesUntilLoop < numSamples) {
                
                // subtract loop length from current position
                thePosition -= loopRange.getLength();
                
                // correct position by the time until the loop
                // otherwise position exceeds loop start and tiggers clips outside the loop
                thePosition += result.timeUntilLoop;
                jassert(thePosition >= 0.0);
                loopCount++;
                result.loopEvent = true;
            }
        }
        else if (loopRange.contains(thePosition)) {
            if (not withinLoop) {
                withinLoop = true;
                NullCheckedInvocation::invoke (onLoopEnteredFunction);
                pendingLoopEntries.fetch_add (1);
                triggerAsyncUpdate();
            }

            // how far the wrap is, for whoever wants to prepare for it
            auto secondsUntilEnd = loopRange.getEnd() - thePosition;
            if (context == audium::clocks)
                secondsUntilEnd = tempoProvider->clocksToSeconds(secondsUntilEnd);
            result.numSamplesUntilLoopEnd = static_cast<int>(std::round(secondsUntilEnd * externalSampleRate));
        }
        else {
            withinLoop = false;
        }
    }
    else {
        withinLoop = false;
    }
    
    result.positionResult = thePosition;
    
    if (context == audium::seconds) {
        currentPositionClocks = tempoProvider->secondsToClocks(thePosition);
    }
    else {
        currentPositionClocks = thePosition;
    }
    
    if (result.loopEvent) {
        NullCheckedInvocation::invoke (onLoopActionFunction);
        pendingLoopWraps.fetch_add (1);
        triggerAsyncUpdate();
    }
    
    
    NullCheckedInvocation::invoke (onPlayListItemUpdateFunction);
    
    return result;
}

void TransportLoop::handleAsyncUpdate()
{
    // Message thread. Entering the loop always precedes the wraps of that
    // pass, so the entries go out first. Events that arrive while this
    // drains are picked up here or by the update they trigger next.
    for (auto n = pendingLoopEntries.exchange (0); n > 0; --n)
        tempoProvider->sendActionMessage (audium::transportLoopEntered);

    for (auto n = pendingLoopWraps.exchange (0); n > 0; --n)
        tempoProvider->sendActionMessage (audium::transportLoopAction);
}

void TransportLoop::reset()
{
    loopCount = 0;
    withinLoop = false;
}

void TransportLoop::setAbsoluteStartPosition(double newPosition, audium::TimeContextType context)
{
    auto positionClocks = 0.0;
    if (context == audium::clocks) {
        positionClocks = newPosition;
    }
    else if (context == audium::seconds) {
        positionClocks = tempoProvider->secondsToClocks(newPosition);
    }
    
    auto loopRange = getLoopPositionRange(audium::clocks);
    
    if (loopRange.contains(positionClocks)) {
        withinLoop = true;
    }
    
}

double TransportLoop::getCurrentPosition(audium::TimeContextType context) const noexcept
{
    if (context == audium::clocks) {
        return currentPositionClocks;
    }
    else if (context == audium::seconds) {
        return tempoProvider->clocksToSeconds(currentPositionClocks);
    }
    jassertfalse;
    return 0.0;
}

double TransportLoop::getLoopPhaseForPosition(double startPosition,
                                              double duration,
                                              audium::TimeContextType context) const
{
    auto loopRange = getLoopPositionRange(context);
    if (loopRange.getLength() > 0.0) {
        auto durationInLoop = startPosition + duration - loopRange.getStart();
        return durationInLoop / loopRange.getLength();
    }
    jassertfalse;
    return 0.0;
}

} // namespace audium
