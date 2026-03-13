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
            
            if (loopData.loopActive) {
                if (loopCount > 0) {
                    auto oldRange = getLoopPositionRange(audium::clocks);
                    auto loopDuration = loopCount * oldRange.getLength();
                    loopCount = loopDuration / newRange.getLength();
                }
            }
            loopData.loopStartPositionClocks = newRange.getStart();
            loopData.loopEndPositionClocks = newRange.getEnd();
            
            
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
    juce::Range<double> range(loopData.loopStartPositionClocks,
                              loopData.loopEndPositionClocks);
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
}

const TransportLoop::LoopResult TransportLoop::processLoop(double thePosition,
                                                           int numSamples,
                                                           audium::TimeContextType context)
{
    TransportLoop::LoopResult result;
    result.context = context;
    
    auto loopRange = getLoopPositionRange(context);
    
    jassert(externalSampleRate > 0.0);
    auto thisBuffer = static_cast<double>(numSamples) / externalSampleRate;
    
    if (context == audium::clocks)
        thisBuffer = tempoProvider->secondsToClocks(thisBuffer);
    
    // subtract previous loops
    thePosition -= (static_cast<double>(loopCount) * loopRange.getLength());
    // jassert(thePosition >= 0.0);
    
    if (loopData.loopActive) {
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
                tempoProvider->sendActionMessage(audium::transportLoopEntered);
            }
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
        tempoProvider->sendActionMessage(audium::transportLoopAction);
    }
    
    
    NullCheckedInvocation::invoke (onPlayListItemUpdateFunction);
    
    return result;
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
